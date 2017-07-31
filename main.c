/*
 * UDP gateway for discrete input/output
 */

#include "main.h"

char pid_path[LINE_SIZE];
char i2c_path[LINE_SIZE];
char hostname[HOST_NAME_MAX];
int app_state = APP_INIT;
int sock_port = -1;
int sock_fd = -1; //udp socket file descriptor
int sock_fd_tf = -1;
int pid_file = -1;
size_t sock_buf_size = 0;
int proc_id = -1;
struct timespec cycle_duration = {0, 0};

PeerList peer_list = {NULL, 0};
Peer peer_client = {.fd = &sock_fd, .addr_size = sizeof peer_client.addr};
char peer_lock_id[NAME_SIZE];
int use_lock = 0;
char device_name[NAME_SIZE];
char db_data_path[LINE_SIZE];
char db_public_path[LINE_SIZE];

pthread_t thread;
char thread_cmd;

I1List i1l = {NULL, 0};
I2List i2l = {NULL, 0};
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
        fputs("ERROR: readSettings: fopen\n", stderr);
#endif
        return 0;
    }
    skipLine(stream);
    int n;
    n = fscanf(stream, "%d\t%255s\t%d\t%ld\t%ld\t%32s\t%d\t%255s\t%32s\t%255s\t%255s\n",
            &sock_port,
            pid_path,
            &sock_buf_size,
            &cycle_duration.tv_sec,
            &cycle_duration.tv_nsec,
            peer_lock_id,
            &use_lock,
            i2c_path,
            device_name,
            db_data_path,
            db_public_path
            );
    if (n != 11) {
        fclose(stream);
#ifdef MODE_DEBUG
        fputs("ERROR: readSettings: bad row format\n", stderr);
#endif
        return 0;
    }
    fclose(stream);
#ifdef MODE_DEBUG
    printf("readSettings: \n\tsock_port: %d, \n\tpid_path: %s, \n\tsock_buf_size: %d, \n\tcycle_duration: %ld sec %ld nsec, \n\tpeer_lock_id: %s, \n\tuse_lock: %d, \n\ti2c_path: %s, \n\tdevice_name: %s, \n\tdb_data_path: %s, \n\tdb_public_path: %s\n",
            sock_port, pid_path, sock_buf_size, cycle_duration.tv_sec, cycle_duration.tv_nsec, peer_lock_id, use_lock, i2c_path, device_name, db_data_path, db_public_path);
#endif
    return 1;
}

void initApp() {
    readHostName(hostname);
#ifdef MODE_DEBUG
    printf("initApp: \n\tCONFIG_FILE: %s\n", CONFIG_FILE);
#endif
    if (!readSettings()) {
        exit_nicely_e("initApp: failed to read settings\n");
    }
    peer_client.sock_buf_size = sock_buf_size;
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
    if (!config_getPeerList(&peer_list, &sock_fd_tf, sock_buf_size, db_public_path)) {
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
    i1l.item = (int *) malloc(sock_buf_size * sizeof *(i1l.item));
    if (i1l.item == NULL) {
        FREE_LIST(&pin_list);
        FREE_LIST(&device_list);
        FREE_LIST(&peer_list);
        return 0;
    }
    i2l.item = (I2 *) malloc(sock_buf_size * sizeof *(i2l.item));
    if (i2l.item == NULL) {
        FREE_LIST(&i1l);
        FREE_LIST(&pin_list);
        FREE_LIST(&device_list);
        FREE_LIST(&peer_list);
        return 0;
    }
    if (!createThread_ctl()) {
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
    char buf_in[sock_buf_size];
    char buf_out[sock_buf_size];
    memset(buf_in, 0, sizeof buf_in);
    acp_initBuf(buf_out, sizeof buf_out);
    if (recvfrom(sock_fd, buf_in, sizeof buf_in, 0, (struct sockaddr*) (&(peer_client.addr)), &(peer_client.addr_size)) < 0) {
#ifdef MODE_DEBUG
        perror("serverRun: recvfrom() error");
#endif
        return;
    }
#ifdef MODE_DEBUG
    acp_dumpBuf(buf_in, sizeof buf_in);
#endif
    if (!acp_crc_check(buf_in, sizeof buf_in)) {
#ifdef MODE_DEBUG
        fputs("serverRun: crc check failed\n", stderr);
#endif
        return;
    }
    switch (buf_in[1]) {
        case ACP_CMD_APP_START:
            if (!init_state) {
                *state = APP_INIT_DATA;
            }
            return;
        case ACP_CMD_APP_STOP:
            if (init_state) {
                *state = APP_STOP;
            }
            return;
        case ACP_CMD_APP_RESET:
            *state = APP_RESET;
            return;
        case ACP_CMD_APP_EXIT:
            *state = APP_EXIT;
            return;
        case ACP_CMD_APP_PING:
            if (init_state) {
                sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_APP_BUSY);
            } else {
                sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_APP_IDLE);
            }
            return;
        case ACP_CMD_APP_PRINT:
            printData(&device_list, &pin_list);
            return;
        case ACP_CMD_APP_HELP:
            printHelp();
            return;
        default:
            if (!init_state) {
                return;
            }
            break;
    }

    switch (buf_in[0]) {
        case ACP_QUANTIFIER_BROADCAST:
        case ACP_QUANTIFIER_SPECIFIC:
            break;
        default:
            return;
    }

    switch (buf_in[1]) {
        case ACP_CMD_GET_INT:
        case ACP_CMD_GWU74_GET_OUT:
        case ACP_CMD_GWU74_GET_DATA:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                    break;
                case ACP_QUANTIFIER_SPECIFIC:
                    acp_parsePackI1(buf_in, &i1l, pin_list.length);
                    if (i1l.length <= 0) {
                        return;
                    }
                    break;
            }

            break;
        case ACP_CMD_SET_INT:
        case ACP_CMD_SET_DUTY_CYCLE_PWM:
        case ACP_CMD_SET_PWM_PERIOD:
        case ACP_CMD_GWU74_SET_RSL:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                    acp_parsePackI1(buf_in, &i1l, pin_list.length);
                    if (i1l.length <= 0) {
                        return;
                    }
                    break;
                case ACP_QUANTIFIER_SPECIFIC:
                    acp_parsePackI2(buf_in, &i2l, pin_list.length);
                    if (i2l.length <= 0) {
                        return;
                    }
                    break;
            }
            break;
        default:
            return;
    }
#ifdef MODE_DEBUG
    char *cmd_str = getCmdStrLocal(buf_in[1]);
    printf("serverRun: local command: %s\n", cmd_str);
#endif
    static int i;
    switch (buf_in[1]) {
        case ACP_CMD_GWU74_GET_DATA:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                    for (i = 0; i < pin_list.length; i++) {
                        if (lockPD(&pin_list.item[i])) {
                            int done = 0;
                            if (!bufCatData(&pin_list.item[i], buf_out, sock_buf_size)) {
                                sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_BUF_OVERFLOW);
                                done = 1;
                            }
                            unlockPD(&pin_list.item[i]);
                            if (done) {
                                return;
                            }
                        }
                    }
                    break;
                case ACP_QUANTIFIER_SPECIFIC:
                    for (i = 0; i < i1l.length; i++) {
                        Pin *p = getPinBy_net_id(i1l.item[i], &pin_list);
                        if (p != NULL) {
                            int done = 0;
                            if (lockPD(&pin_list.item[i])) {
                                if (!bufCatData(p, buf_out, sock_buf_size)) {
                                    sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_BUF_OVERFLOW);
                                    done = 1;
                                }
                                unlockPD(&pin_list.item[i]);
                            }
                            if (done) {
                                return;
                            }
                        }


                    }
                    break;
            }
            break;
        case ACP_CMD_GET_INT:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                    for (i = 0; i < pin_list.length; i++) {
                        if (pin_list.item[i].mode == DIO_MODE_IN) {
                            if (lockPD(&pin_list.item[i])) {
                                getIn(&pin_list.item[i]);
                                unlockPD(&pin_list.item[i]);
                            }
                        }
                    }
                    if (lockPDAll(&device_list, &pin_list)) {
                        readDeviceList(&device_list, &pin_list);
                        unlockPDAll(&device_list, &pin_list);
                    }
                    for (i = 0; i < pin_list.length; i++) {
                        if (lockPD(&pin_list.item[i])) {
                            int done = 0;
                            if (!bufCatPinIn(&pin_list.item[i], buf_out, sock_buf_size)) {
                                sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_BUF_OVERFLOW);
                                done = 1;
                            }
                            unlockPD(&pin_list.item[i]);
                            if (done) {
                                return;
                            }
                        }
                    }
                    break;
                case ACP_QUANTIFIER_SPECIFIC:
                    for (i = 0; i < i1l.length; i++) {
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
                    for (i = 0; i < i1l.length; i++) {

                        Pin *p = getPinBy_net_id(i1l.item[i], &pin_list);
                        if (p != NULL) {
                            if (lockPD(&pin_list.item[i])) {
                                int done = 0;
                                if (!bufCatPinIn(p, buf_out, sock_buf_size)) {
                                    sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_BUF_OVERFLOW);
                                    done = 1;
                                }
                                unlockPD(&pin_list.item[i]);
                                if (done) {
                                    return;
                                }
                            }
                        }
                    }
                    break;
            }
            break;
        case ACP_CMD_GWU74_GET_OUT:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                    for (i = 0; i < pin_list.length; i++) {
                        if (lockPin(&pin_list.item[i])) {
                            int done = 0;
                            if (!bufCatPinOut(&pin_list.item[i], buf_out, sock_buf_size)) {
                                sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_BUF_OVERFLOW);
                                done = 1;
                            }
                            unlockPin(&pin_list.item[i]);
                            if (done) {
                                return;
                            }
                        }
                    }
                    break;
                case ACP_QUANTIFIER_SPECIFIC:
                    for (i = 0; i < i1l.length; i++) {
                        Pin *p = getPinBy_net_id(i1l.item[i], &pin_list);
                        if (p != NULL) {
                            if (lockPin(&pin_list.item[i])) {
                                int done = 0;
                                if (!bufCatPinOut(p, buf_out, sock_buf_size)) {
                                    sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_BUF_OVERFLOW);
                                    done = 1;
                                }
                                unlockPin(&pin_list.item[i]);
                                if (done) {
                                    return;
                                }
                            }
                        }
                    }
                    break;
            }
            break;
        case ACP_CMD_SET_INT:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                    for (i = 0; i < pin_list.length; i++) {
                        setPinOutput(&pin_list.item[i], i1l.item[0]);
                    }
                    break;
                case ACP_QUANTIFIER_SPECIFIC:
                    for (i = 0; i < i2l.length; i++) {
                        Pin *p = getPinBy_net_id(i2l.item[i].p0, &pin_list);
                        if (p != NULL) {
                            setPinOutput(p, i2l.item[i].p1);
                        }
                    }
                    break;
            }
            return;
        case ACP_CMD_SET_DUTY_CYCLE_PWM:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                    for (i = 0; i < pin_list.length; i++) {
                        setPinDutyCyclePWM(&pin_list.item[i], i1l.item[0]);
                    }
                    break;
                case ACP_QUANTIFIER_SPECIFIC:
                    for (i = 0; i < i2l.length; i++) {
                        Pin *p = getPinBy_net_id(i2l.item[i].p0, &pin_list);
                        if (p != NULL) {
                            setPinDutyCyclePWM(p, i2l.item[i].p1);
                        }
                    }
                    break;
            }
            return;
        case ACP_CMD_SET_PWM_PERIOD:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                    for (i = 0; i < pin_list.length; i++) {
                        setPinPeriodPWM(&pin_list.item[i], i1l.item[0], db_data_path);
                    }
                    break;
                case ACP_QUANTIFIER_SPECIFIC:
                    for (i = 0; i < i2l.length; i++) {
                        Pin *p = getPinBy_net_id(i2l.item[i].p0, &pin_list);
                        if (p != NULL) {
                            setPinPeriodPWM(p, i2l.item[i].p1, db_data_path);
                        }
                    }
                    break;
            }
            return;
        case ACP_CMD_GWU74_SET_RSL:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                    for (i = 0; i < pin_list.length; i++) {
                        setPinRslPWM(&pin_list.item[i], i1l.item[0], db_data_path);
                    }
                    break;
                case ACP_QUANTIFIER_SPECIFIC:
                    for (i = 0; i < i2l.length; i++) {
                        Pin *p = getPinBy_net_id(i2l.item[i].p0, &pin_list);
                        if (p != NULL) {
                            setPinRslPWM(p, i2l.item[i].p1, db_data_path);
                        }
                    }
                    break;
            }
            return;
    }


    switch (buf_in[1]) {
        case ACP_CMD_GET_INT:
        case ACP_CMD_GWU74_GET_OUT:
        case ACP_CMD_GWU74_GET_DATA:
            if (!sendBufPack(buf_out, ACP_QUANTIFIER_SPECIFIC, ACP_RESP_REQUEST_SUCCEEDED)) {
                sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_BUF_OVERFLOW);
            }
            return;
    }

}

void updateOutSafe(PinList *list) {
    size_t i;
    for (i = 0; i < list->length; i++) {
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
    char *cmd = (char *) arg;
#ifndef MODE_DEBUG
    // setPriorityMax(SCHED_FIFO);
#endif
    size_t i;
    for (i = 0; i < pin_list.length; i++) {
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
        size_t i;
        struct timespec t1 = getCurrentTime();
        for (i = 0; i < pin_list.length; i++) {
            if (pin_list.item[i].mode == DIO_MODE_OUT && pin_list.item[i].out_pwm) {
                int v = pwmctl(&pin_list.item[i].pwm, pin_list.item[i].duty_cycle);
                if (tryLockPD(&pin_list.item[i])) {
                    setPinOut(&pin_list.item[i], v);
                    unlockPD(&pin_list.item[i]);
                }
            }
        }

        //writing to chips if data changed
        app_writeDeviceList(&device_list);

        switch (*cmd) {
            case ACP_CMD_APP_STOP:
            case ACP_CMD_APP_RESET:
            case ACP_CMD_APP_EXIT:
                updateOutSafe(&pin_list);
                app_writeDeviceList(&device_list);
                *cmd = ACP_CMD_APP_NO;
                return (EXIT_SUCCESS);
            default:
                break;
        }
        sleepRest(cycle_duration, t1);

    }
}

int createThread_ctl() {
    if (pthread_create(&thread, NULL, &threadFunction, (void *) &thread_cmd) != 0) {
        perror("createThreads: pthread_create");
        return 0;
    }
    return 1;
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
        size_t i;
        for (i = 0; i < pl->length; i++) {
            done = done && initMutex(&pl->item[i].mutex);
            if (!done) {
                break;
            }
        }
        for (i = 0; i < dl->length; i++) {
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
    waitThread_ctl(ACP_CMD_APP_EXIT);
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

