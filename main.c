#include "main.h"

char pid_path[LINE_SIZE];
char i2c_path[LINE_SIZE];

int app_state = APP_INIT;
int sock_port = -1;
int sock_fd = -1; //udp socket file descriptor
int sock_fd_tf = -1;
int pid_file = -1;
int proc_id = -1;
struct timespec cycle_duration = {0, 0};

PeerList peer_list = {NULL, 0};
Peer peer_client = {.fd = &sock_fd, .addr_size = sizeof peer_client.addr};
char peer_lock_id[NAME_SIZE];
int use_lock = 0;
char device_name[NAME_SIZE];
char db_data_path[LINE_SIZE];
char db_public_path[LINE_SIZE];

DEF_THREAD

I1List i1l;
I2List i2l;
DeviceList device_list = {NULL, 0};
PinList pin_list = {NULL, 0};

void (*setMode)(Pin *, int);
void (*setPUD)(Pin *, int);
void (*setOut)(Pin *, int);
void (*getIn)(Pin *);
void (*writeDeviceList)(DeviceList *);
void (*readDeviceList)(DeviceList *, PinList *);

#include "device/main.h"
#include "util.c"
#include "device/common.c"
#include "device/idle.c"
#include "device/native.c"
#include "device/pcf8574.c"
#include "device/mcp23008.c"
#include "device/mcp23017.c"
#include "print.c"

int readSettings() {
    FILE* stream = fopen(CONFIG_FILE, "r");
    if (stream == NULL) {
#ifdef MODE_DEBUG
        perror("readSettings()");
#endif
        return 0;
    }
    skipLine(stream);
    int n;
    n = fscanf(stream, "%d\t%255s\t%ld\t%ld\t%32s\t%d\t%255s\t%32s\t%255s\t%255s\n",
            &sock_port,
            pid_path,
            &cycle_duration.tv_sec,
            &cycle_duration.tv_nsec,
            peer_lock_id,
            &use_lock,
            i2c_path,
            device_name,
            db_data_path,
            db_public_path
            );
    if (n != 10) {
        fclose(stream);
#ifdef MODE_DEBUG
        fputs("ERROR: readSettings: bad row format\n", stderr);
#endif
        return 0;
    }
    fclose(stream);
#ifdef MODE_DEBUG
    printf("readSettings: \n\tsock_port: %d, \n\tpid_path: %s, \n\tcycle_duration: %ld sec %ld nsec, \n\tpeer_lock_id: %s, \n\tuse_lock: %d, \n\ti2c_path: %s, \n\tdevice_name: %s, \n\tdb_data_path: %s, \n\tdb_public_path: %s\n",
            sock_port, pid_path, cycle_duration.tv_sec, cycle_duration.tv_nsec, peer_lock_id, use_lock, i2c_path, device_name, db_data_path, db_public_path);
#endif
    return 1;
}

void initApp() {
#ifdef MODE_DEBUG
    printf("initApp: \n\tCONFIG_FILE: %s\n", CONFIG_FILE);
#endif
    if (!readSettings()) {
        exit_nicely_e("initApp: failed to read settings\n");
    }
    if (!initPid(&pid_file, &proc_id, pid_path)) {
        exit_nicely_e("initApp: failed to initialize pid\n");
    }
    if (!initServer(&sock_fd, sock_port)) {
        exit_nicely_e("initApp: failed to initialize udp server\n");
    }
    if (!initClient(&sock_fd_tf, WAIT_RESP_TIMEOUT)) {
        exit_nicely_e("initApp: failed to initialize udp client\n");
    }

}

int initData() {
    if (!config_getPeerList(&peer_list, &sock_fd_tf, db_public_path)) {
        FREE_LIST(&peer_list);
        return 0;
    }
    if (use_lock) {
        Peer *peer_lock = NULL;
        peer_lock = getPeerById(peer_lock_id, &peer_list);
        if (peer_lock == NULL) {
            FREE_LIST(&peer_list);
            return 0;
        }
        acp_lck_waitUnlock(peer_lock, LOCK_COM_INTERVAL);
    }
    if (!initDevice(&device_list, &pin_list, device_name)) {
        FREE_LIST(&pin_list);
        FREE_LIST(&device_list);
        FREE_LIST(&peer_list);
        return 0;
    }
    if (!initI1List(&i1l, pin_list.length)) {
        FREE_LIST(&pin_list);
        FREE_LIST(&device_list);
        FREE_LIST(&peer_list);
        return 0;
    }
    if (!initI2List(&i2l, pin_list.length)) {
        FREE_LIST(&i1l);
        FREE_LIST(&pin_list);
        FREE_LIST(&device_list);
        FREE_LIST(&peer_list);
        return 0;
    }
    if (!THREAD_CREATE) {
        FREE_LIST(&i2l);
        FREE_LIST(&i1l);
        FREE_LIST(&pin_list);
        FREE_LIST(&device_list);
        FREE_LIST(&peer_list);
        return 0;
    }
    return 1;
}

void serverRun(int *state, int init_state) {
    SERVER_HEADER
    SERVER_APP_ACTIONS
    if (
            ACP_CMD_IS(ACP_CMD_GET_ITS) ||
            ACP_CMD_IS(ACP_CMD_GWU74_GET_OUT) ||
            ACP_CMD_IS(ACP_CMD_PROG_GET_DATA_INIT)
            ) {
        acp_requestDataToI1List(&request, &i1l);
        if (i1l.length <= 0) {
            return;
        }
    } else if (
            ACP_CMD_IS(ACP_CMD_SET_INT) ||
            ACP_CMD_IS(ACP_CMD_SET_PWM_DUTY_CYCLE) ||
            ACP_CMD_IS(ACP_CMD_SET_PWM_PERIOD) ||
            ACP_CMD_IS(ACP_CMD_GWU74_SET_RSL)
            ) {
        acp_requestDataToI2List(&request, &i2l);
        if (i2l.length <= 0) {
            return;
        }
    } else {
        return;
    }
    if (ACP_CMD_IS(ACP_CMD_PROG_GET_DATA_INIT)) {
        for (int i = 0; i < i1l.length; i++) {
            Pin *p = getPinBy_net_id(i1l.item[i], &pin_list);
            if (p != NULL) {
                int done = 0;
                if (lockPD(&pin_list.item[i])) {
                    done = bufCatDataInit(p, &response);
                    unlockPD(&pin_list.item[i]);
                }
                if (!done) {
                    return;
                }
            }
        }
    } else if (ACP_CMD_IS(ACP_CMD_GET_ITS)) {
        for (int i = 0; i < i1l.length; i++) {
            Pin *p = getPinBy_net_id(i1l.item[i], &pin_list);
            if (p != NULL) {
                if (p->mode == DIO_MODE_IN) {
                    getIn(p);
                }
            }
        }
        if (lockPDAll(&device_list, &pin_list)) {
            readDeviceList(&device_list, &pin_list);
            unlockPDAll(&device_list, &pin_list);
        }
        for (int i = 0; i < i1l.length; i++) {
            Pin *p = getPinBy_net_id(i1l.item[i], &pin_list);
            if (p != NULL) {
                int done = 0;
                if (lockPD(&pin_list.item[i])) {
                    done = bufCatPinIn(p, &response);
                    unlockPD(&pin_list.item[i]);
                }
                if (!done) {
                    return;
                }
            }
        }
    } else if (ACP_CMD_IS(ACP_CMD_GWU74_GET_OUT)) {
        for (int i = 0; i < i1l.length; i++) {
            Pin *p = getPinBy_net_id(i1l.item[i], &pin_list);
            if (p != NULL) {
                int done = 0;
                if (lockPin(&pin_list.item[i])) {
                    done = bufCatPinOut(p, &response);
                    unlockPin(&pin_list.item[i]);
                }
                if (!done) {
                    return;
                }
            }
        }
    } else if (ACP_CMD_IS(ACP_CMD_SET_INT)) {
        for (int i = 0; i < i2l.length; i++) {
            Pin *p = getPinBy_net_id(i2l.item[i].p0, &pin_list);
            if (p != NULL) {
                if (lockPin(p)) {
                    setPinOutput(p, i2l.item[i].p1);
                    resetSecure(&p->secure_out);
                    unlockPin(p);
                }
            }
        }
        return;
    } else if (ACP_CMD_IS(ACP_CMD_SET_PWM_DUTY_CYCLE)) {
        for (int i = 0; i < i2l.length; i++) {
            Pin *p = getPinBy_net_id(i2l.item[i].p0, &pin_list);
            if (p != NULL) {
                if (lockPin(p)) {
                    setPinDutyCyclePWM(p, i2l.item[i].p1);
                    resetSecure(&p->secure_out);
                    unlockPin(p);
                }
            }
        }
        return;
    } else if (ACP_CMD_IS(ACP_CMD_SET_PWM_PERIOD)) {
        for (int i = 0; i < i2l.length; i++) {
            Pin *p = getPinBy_net_id(i2l.item[i].p0, &pin_list);
            if (p != NULL) {
                setPinPeriodPWM(p, i2l.item[i].p1, db_data_path);
            }
        }
        return;
    } else if (ACP_CMD_IS(ACP_CMD_GWU74_SET_RSL)) {
        for (int i = 0; i < i2l.length; i++) {
            Pin *p = getPinBy_net_id(i2l.item[i].p0, &pin_list);
            if (p != NULL) {
                setPinRslPWM(p, i2l.item[i].p1, db_data_path);
            }
        }
        return;
    }
    acp_responseSend(&response, &peer_client);
}

void updateOutSafe(PinList *list) {
    for (int i = 0; i < list->length; i++) {
        if (lockPD(&pin_list.item[i])) {
            switch (list->item[i].mode) {
                case DIO_MODE_IN:
                    break;
                case DIO_MODE_OUT:
                    setOut(&list->item[i], DIO_HIGH);
                    list->item[i].out = DIO_HIGH;
                    break;
            }
            unlockPD(&pin_list.item[i]);
        }
    }
}

void *threadFunction(void *arg) {
    THREAD_DEF_CMD
#ifndef MODE_DEBUG
            // setPriorityMax(SCHED_FIFO);
#endif
            for (int i = 0; i < pin_list.length; i++) {
        if (lockPD(&pin_list.item[i])) {
            setMode(&pin_list.item[i], pin_list.item[i].mode);
            setPUD(&pin_list.item[i], pin_list.item[i].pud);
            unlockPD(&pin_list.item[i]);
        }
    }
    updateOutSafe(&pin_list);
    app_writeDeviceList(&device_list);

#ifdef MODE_DEBUG
    puts("threadFunction: entering while(1) cycle...");
#endif
    while (1) {
        struct timespec t1 = getCurrentTime();
        for (int i = 0; i < pin_list.length; i++) {
            if (pin_list.item[i].mode == DIO_MODE_OUT) {
                if (lockPin(&pin_list.item[i])) {
                    if (needSecure(&pin_list.item[i].secure_out)) {
                        pin_list.item[i].out_pwm = 1;
                        pin_list.item[i].duty_cycle = pin_list.item[i].secure_out.duty_cycle;
                        doneSecure(&pin_list.item[i].secure_out);
                    }
                    if (pin_list.item[i].out_pwm) {
                        int v = pwmctl(&pin_list.item[i].pwm, pin_list.item[i].duty_cycle);
                        setPinOut(&pin_list.item[i], v);
                    }
                    unlockPin(&pin_list.item[i]);
                }
            }
        }
        //writing to chips if data changed
        app_writeDeviceList(&device_list);

        if (*cmd) {
            updateOutSafe(&pin_list);
            app_writeDeviceList(&device_list);
            *cmd = 0;
            return (EXIT_SUCCESS);
        }
        sleepRest(cycle_duration, t1);

    }
}

int initDevice(DeviceList *dl, PinList *pl, char *device) {
    int done = 0;
    if (strcmp(DEVICE_MCP23008_STR, device) == 0) {
        done = mcp23008_initDevPin(dl, pl, db_data_path, i2c_path);
    } else if (strcmp(DEVICE_MCP23017_STR, device) == 0) {
        done = mcp23017_initDevPin(dl, pl, db_data_path, i2c_path);
    } else if (strcmp(DEVICE_PCF8574_STR, device) == 0) {
        done = pcf8574_initDevPin(dl, pl, db_data_path, i2c_path);
    } else if (strcmp(DEVICE_NATIVE_STR, device) == 0) {
        done = native_initDevPin(dl, pl, db_data_path);
    } else if (strcmp(DEVICE_IDLE_STR, device) == 0) {
        done = idle_initDevPin(dl, pl, db_data_path);
    }
    if (done) {
        for (int i = 0; i < pl->length; i++) {
            done = done && initMutex(&pl->item[i].mutex);
            if (!done) {
                break;
            }
        }
        for (int i = 0; i < dl->length; i++) {
            done = done && initMutex(&dl->item[i].mutex);
            if (!done) {
                break;
            }
        }
    }
    return done;
}

void freeData() {
    if (use_lock) {
        Peer *peer_lock = NULL;
        peer_lock = getPeerById(peer_lock_id, &peer_list);
        if (peer_lock != NULL) {
            acp_lck_lock(peer_lock);
        }
    }
    THREAD_STOP
    FREE_LIST(&i2l);
    FREE_LIST(&i1l);
    FREE_LIST(&pin_list);
    FREE_LIST(&device_list);
    FREE_LIST(&peer_list);
}

void freeApp() {
    freeData();
    freeSocketFd(&sock_fd);
    freeSocketFd(&sock_fd_tf);
    freePid(&pid_file, &proc_id, pid_path);
}

void exit_nicely() {
    freeApp();
#ifdef MODE_DEBUG
    puts("\nBye...");
#endif
    exit(EXIT_SUCCESS);
}

void exit_nicely_e(char *s) {
    fprintf(stderr, "%s", s);
    freeApp();
    exit(EXIT_FAILURE);
}

int main(int argc, char** argv) {
    if (geteuid() != 0) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s: root user expected\n", APP_NAME_STR);
#endif
        return (EXIT_FAILURE);
    }
#ifndef MODE_DEBUG
    daemon(0, 0);
#endif
    conSig(&exit_nicely);
    if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
        perror("main: memory locking failed");
    }
#ifndef MODE_DEBUG
    //  setPriorityMax(SCHED_FIFO);
#endif
    int data_initialized = 0;
    while (1) {
        switch (app_state) {
            case APP_INIT:
#ifdef MODE_DEBUG
                puts("MAIN: init");
#endif
                initApp();
                app_state = APP_INIT_DATA;
                break;
            case APP_INIT_DATA:
#ifdef MODE_DEBUG
                puts("MAIN: init data");
#endif
                data_initialized = initData();
                app_state = APP_RUN;
                delayUsIdle(1000);
                break;
            case APP_RUN:
#ifdef MODE_DEBUG
                puts("MAIN: run");
#endif
                serverRun(&app_state, data_initialized);
                break;
            case APP_STOP:
#ifdef MODE_DEBUG
                puts("MAIN: stop");
#endif
                freeData();
                data_initialized = 0;
                app_state = APP_RUN;
                break;
            case APP_RESET:
#ifdef MODE_DEBUG
                puts("MAIN: reset");
#endif
                freeApp();
                delayUsIdle(1000000);
                data_initialized = 0;
                app_state = APP_INIT;
                break;
            case APP_EXIT:
#ifdef MODE_DEBUG
                puts("MAIN: exit");
#endif
                exit_nicely();
                break;
            default:
                exit_nicely_e("main: unknown application state");
                break;
        }
    }
    freeApp();
    return (EXIT_SUCCESS);
}

