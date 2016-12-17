/*
 * UDP gateway for discrete input/output
 */

#include "main.h"

char app_class[NAME_SIZE];
char pid_path[LINE_SIZE];
char i2c_path[LINE_SIZE];
char hostname[HOST_NAME_MAX];
int app_state = APP_INIT;
int udp_port = -1;
int udp_fd = -1; //udp socket file descriptor
int pid_file = -1;
size_t udp_buf_size = 0;
int proc_id = -1;
struct timespec cycle_duration = {0, 0};
int data_initialized = 0;

Peer peer_client = {.fd = &udp_fd, .addr_size = sizeof peer_client.addr};

char db_conninfo_settings[LINE_SIZE];
char db_conninfo_data[LINE_SIZE];
char device_name[NAME_SIZE];
PGconn *db_conn_settings = NULL;
PGconn *db_conn_data = NULL;
PGconn **db_connp_data = NULL;
int db_init_data = 0;
ThreadData thread_data = {.cmd = ACP_CMD_APP_NO, .on = 0};
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
#include "device/idle.c"
#include "device/native.c"
#include "device/pcf8574.c"
#include "device/mcp23008.c"
#include "device/mcp23017.c"
#include "print.c"

int readSettings() {
    PGresult *r;
    char q[LINE_SIZE];
    memset(pid_path, 0, sizeof pid_path);
    memset(i2c_path, 0, sizeof i2c_path);
    memset(db_conninfo_data, 0, sizeof db_conninfo_data);

    snprintf(q, sizeof q, "select db_public from "APP_NAME_STR".config where app_class='%s'", app_class);
    if ((r = dbGetDataT(db_conn_settings, q, "readSettings: select: ")) == NULL) {
        return 0;
    }
    if (PQntuples(r) != 1) {
        PQclear(r);
        fputs("readSettings: need only one tuple (1)\n", stderr);
        return 0;
    }
    char db_conninfo_public[LINE_SIZE];
    PGconn *db_conn_public = NULL;
    PGconn **db_connp_public = NULL;
    memcpy(db_conninfo_public, PQgetvalue(r, 0, 0), LINE_SIZE);
    PQclear(r);
    if (dbConninfoEq(db_conninfo_public, db_conninfo_settings)) {
        db_connp_public = &db_conn_settings;
    } else {
        if (!initDB(&db_conn_public, db_conninfo_public)) {
            return 0;
        }
        db_connp_public = &db_conn_public;
    }

    snprintf(q, sizeof q, "select udp_port, i2c_path, pid_path, udp_buf_size, db_data, cycle_duration_us, device_name from "APP_NAME_STR".config where app_class='%s'", app_class);
    if ((r = dbGetDataT(db_conn_settings, q, "readSettings: select: ")) == NULL) {
        return 0;
    }
    if (PQntuples(r) == 1) {
        int done = 1;
        done = done && config_getUDPPort(*db_connp_public, PQgetvalue(r, 0, 0), &udp_port);
        done = done && config_getI2cPath(*db_connp_public, PQgetvalue(r, 0, 1), i2c_path, LINE_SIZE);
        done = done && config_getPidPath(*db_connp_public, PQgetvalue(r, 0, 2), pid_path, LINE_SIZE);
        done = done && config_getBufSize(*db_connp_public, PQgetvalue(r, 0, 3), &udp_buf_size);
        done = done && config_getDbConninfo(*db_connp_public, PQgetvalue(r, 0, 4), db_conninfo_data, LINE_SIZE);
        done = done && config_getCycleDurationUs(*db_connp_public, PQgetvalue(r, 0, 5), &cycle_duration);
        done = done && config_getStrValFromTbl(*db_connp_public, PQgetvalue(r, 0, 6), device_name, "device_name", NAME_SIZE);
        if (!done) {
            PQclear(r);
            freeDB(&db_conn_public);
            fputs("readSettings: failed to read some fields\n", stderr);
            return 0;
        }
    } else {
        PQclear(r);
        freeDB(&db_conn_public);
        fputs("readSettings: need only one tuple\n", stderr);
        return 0;
    }
    PQclear(r);
    freeDB(&db_conn_public);
    db_init_data = 0;
    if (dbConninfoEq(db_conninfo_data, db_conninfo_settings)) {
        db_connp_data = &db_conn_settings;
    } else {
        db_connp_data = &db_conn_data;
        db_init_data = 1;
    }
    return 1;
}

void serverRun(int *state, int init_state) {
    char buf_in[udp_buf_size];
    char buf_out[udp_buf_size];
    static int i, j;
    static uint8_t crc;
    static char q[LINE_SIZE];
    crc = 0;
    memset(buf_in, 0, sizeof buf_in);
    acp_initBuf(buf_out, sizeof buf_out);
    if (recvfrom(udp_fd, buf_in, sizeof buf_in, 0, (struct sockaddr*) (&(peer_client.addr)), &(peer_client.addr_size)) < 0) {
#ifdef MODE_DEBUG
        perror("serverRun: recvfrom() error");
#endif
        return;
    }
#ifdef MODE_DEBUG
    dumpBuf(buf_in, sizeof buf_in);
#endif
    if (!crc_check(buf_in, sizeof buf_in)) {
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
                if (thread_data.on) {
                    waitThreadCmd(&thread_data.cmd, &thread_data.qfr, buf_in);
                }
                *state = APP_STOP;
            }
            return;
        case ACP_CMD_APP_RESET:
            if (thread_data.on) {
                waitThreadCmd(&thread_data.cmd, &thread_data.qfr, buf_in);
            }
            *state = APP_RESET;
            return;
        case ACP_CMD_APP_EXIT:
            if (thread_data.on) {
                waitThreadCmd(&thread_data.cmd, &thread_data.qfr, buf_in);
            }
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

    switch (buf_in[1]) {
        case ACP_CMD_GWU74_GET_DATA:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                    for (i = 0; i < pin_list.length; i++) {
                        if (lockPD(&pin_list.item[i])) {
                            int done = 0;
                            if (!bufCatData(&pin_list.item[i], buf_out, udp_buf_size)) {
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
                                if (!bufCatData(p, buf_out, udp_buf_size)) {
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
                            if (!bufCatPinIn(&pin_list.item[i], buf_out, udp_buf_size)) {
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
                                if (!bufCatPinIn(p, buf_out, udp_buf_size)) {
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
                            if (!bufCatPinOut(&pin_list.item[i], buf_out, udp_buf_size)) {
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
                                if (!bufCatPinOut(p, buf_out, udp_buf_size)) {
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
                        if (lockPD(&pin_list.item[i])) {
                            setPinOutput(&pin_list.item[i], i1l.item[0]);
                            unlockPD(&pin_list.item[i]);
                        }
                    }
                    break;
                case ACP_QUANTIFIER_SPECIFIC:
                    for (i = 0; i < i2l.length; i++) {
                        Pin *p = getPinBy_net_id(i2l.item[i].p0, &pin_list);
                        if (p != NULL) {
                            if (lockPD(p)) {
                                setPinOutput(p, i2l.item[i].p1);
                                unlockPD(p);
                            }
                        }
                    }
                    break;
            }
            break;
        case ACP_CMD_SET_DUTY_CYCLE_PWM:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                    for (i = 0; i < pin_list.length; i++) {
                        if (lockPin(&pin_list.item[i])) {
                            setPinDutyCyclePWM(&pin_list.item[i], i1l.item[0]);
                            unlockPin(&pin_list.item[i]);
                        }
                    }
                    break;
                case ACP_QUANTIFIER_SPECIFIC:
                    for (i = 0; i < i2l.length; i++) {
                        Pin *p = getPinBy_net_id(i2l.item[i].p0, &pin_list);
                        if (p != NULL) {
                            if (lockPin(p)) {
                                setPinDutyCyclePWM(p, i2l.item[i].p1);
                                unlockPin(p);
                            }
                        }
                    }
                    break;
            }
        case ACP_CMD_SET_PWM_PERIOD:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                    for (i = 0; i < pin_list.length; i++) {
                        if (lockPin(&pin_list.item[i])) {
                            pin_list.item[i].pwm.period.tv_sec = i1l.item[0];
                            savePin(&pin_list.item[i]);
                            unlockPin(&pin_list.item[i]);
                        }
                    }
                    break;
                case ACP_QUANTIFIER_SPECIFIC:
                    for (i = 0; i < i2l.length; i++) {
                        Pin *p = getPinBy_net_id(i2l.item[i].p0, &pin_list);
                        if (p != NULL) {
                            if (lockPin(p)) {
                                p->pwm.period.tv_sec = i2l.item[i].p1;
                                savePin(p);
                                unlockPin(p);
                            }
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
                    break;
            }
            unlockPD(&pin_list.item[i]);
        }
    }
}

void *threadFunction(void *arg) {
    puts("threadFunction: begin");
    ThreadData *data = (ThreadData *) arg;
    data->on = 1;
    int r;
#ifndef MODE_DEBUG
    setPriorityMax(SCHED_FIFO);
#endif
    if (pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &r) != 0) {
        perror("threadFunction: pthread_setcancelstate");
    }
    if (pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &r) != 0) {
        perror("threadFunction: pthread_setcanceltype");
    }
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
        struct timespec t1;
        clock_gettime(LIB_CLOCK, &t1);

        for (i = 0; i < pin_list.length; i++) {
            if (pin_list.item[i].mode == DIO_MODE_OUT && pin_list.item[i].out_pwm) {
                int v = pwmctl(&pin_list.item[i].pwm, pin_list.item[i].duty_cycle);
                if (tryLockPD(&pin_list.item[i])) {
                    setOut(&pin_list.item[i], v);
                    unlockPD(&pin_list.item[i]);
                }
            }
        }

        //writing to chips if data changed
        app_writeDeviceList(&device_list);

        switch (data->cmd) {
            case ACP_CMD_APP_STOP:
            case ACP_CMD_APP_RESET:
            case ACP_CMD_APP_EXIT:
                updateOutSafe(&pin_list);
                app_writeDeviceList(&device_list);
                data->cmd = ACP_CMD_APP_NO;
                data->on = 0;
                return (EXIT_SUCCESS);
            default:
                break;
        }
        data->cmd = ACP_CMD_APP_NO; //notify main thread that command has been executed
        sleepRest(data->cycle_duration, t1);

    }
}

int createThread(ThreadData * td) {
    //set attributes for each thread
    memset(&td->init_success, 0, sizeof td->init_success);
    if (pthread_attr_init(&td->thread_attr) != 0) {
        perror("createThreads: pthread_attr_init");
        return 0;
    }
    td->init_success.thread_attr_init = 1;
    td->cycle_duration = cycle_duration;
    if (pthread_attr_setdetachstate(&td->thread_attr, PTHREAD_CREATE_DETACHED) != 0) {
        perror("createThreads: pthread_attr_setdetachstate");
        return 0;
    }
    td->init_success.thread_attr_setdetachstate = 1;
    //create a thread
    if (pthread_create(&td->thread, &td->thread_attr, threadFunction, (void *) td) != 0) {
        perror("createThreads: pthread_create");
        return 0;
    }
    td->init_success.thread_create = 1;

    return 1;
}

void initApp() {
    readHostName(hostname);
    if (!readConf(CONF_FILE, db_conninfo_settings, app_class)) {
        exit_nicely_e("initApp: failed to read configuration file\n");
    }
    if (!initDB(&db_conn_settings, db_conninfo_settings)) {
        exit_nicely_e("initApp: failed to initialize db\n");
    }
    if (!readSettings()) {
        exit_nicely_e("initApp: failed to read settings\n");
    }
    if (!initPid(&pid_file, &proc_id, pid_path)) {
        exit_nicely_e("initApp: failed to initialize pid\n");
    }
    if (!initUDPServer(&udp_fd, udp_port)) {
        exit_nicely_e("initApp: failed to initialize udp server\n");
    }
    puts("initApp:done");
}

int initDevice(DeviceList *dl, PinList *pl, char *device) {
    puts(device);
    int done = 0;
    if (strcmp(DEVICE_MCP23008_STR, device) == 0) {
        done = mcp23008_initDevPin(dl, pl, *db_connp_data, app_class, i2c_path);
    } else if (strcmp(DEVICE_MCP23017_STR, device) == 0) {
        done = mcp23017_initDevPin(dl, pl, *db_connp_data, app_class, i2c_path);
    } else if (strcmp(DEVICE_PCF8574_STR, device) == 0) {
        done = pcf8574_initDevPin(dl, pl, *db_connp_data, app_class, i2c_path);
    } else if (strcmp(DEVICE_NATIVE_STR, device) == 0) {
        done = native_initDevPin(dl, pl, *db_connp_data, app_class);
    } else if (strcmp(DEVICE_IDLE_STR, device) == 0) {
        done = idle_initDevPin(dl, pl, *db_connp_data, app_class);
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

void initData() {
    data_initialized = 0;
    if (db_init_data) {
        if (!initDB(&db_conn_data, db_conninfo_data)) {
            return;
        }
        puts("initData: db_data initialized");
    }
    puts("initData: db done");
    if (!initDevice(&device_list, &pin_list, device_name)) {
        FREE_LIST(&pin_list);
        FREE_LIST(&device_list);
        freeDB(&db_conn_data);
        return;
    }
    i1l.item = (int *) malloc(udp_buf_size * sizeof *(i1l.item));
    if (i1l.item == NULL) {
        FREE_LIST(&pin_list);
        FREE_LIST(&device_list);
        freeDB(&db_conn_data);
        return;
    }
    i2l.item = (I2 *) malloc(udp_buf_size * sizeof *(i2l.item));
    if (i2l.item == NULL) {
        FREE_LIST(&i1l);
        FREE_LIST(&pin_list);
        FREE_LIST(&device_list);
        freeDB(&db_conn_data);
        return;
    }
    if (!createThread(&thread_data)) {
        freeThread();
        FREE_LIST(&i2l);
        FREE_LIST(&i1l);
        FREE_LIST(&pin_list);
        FREE_LIST(&device_list);
        freeDB(&db_conn_data);
        return;
    }
    data_initialized = 1;
#ifdef MODE_DEBUG
    puts("initData: done");
#endif
}

void freeThread() {
    if (thread_data.init_success.thread_create) {
        if (thread_data.on) {
            char cmd[2];
            cmd[1] = ACP_CMD_APP_EXIT;
            waitThreadCmd(&thread_data.cmd, &thread_data.qfr, cmd);
        }
    } 
    if (thread_data.init_success.thread_attr_init) {
        if (pthread_attr_destroy(&thread_data.thread_attr) != 0) {
            perror("freeThread: pthread_attr_destroy");
        } else {
            thread_data.init_success.thread_attr_init = 0;
        }
    }
    thread_data.init_success.thread_create = 0;
    thread_data.on = 0;
    thread_data.cmd = ACP_CMD_APP_NO;
}

void freeAppData() {
    FREE_LIST(&i2l);
    FREE_LIST(&i1l);
    FREE_LIST(&pin_list);
    FREE_LIST(&device_list);
    freeDB(&db_conn_data);
}

void freeData() {
    freeThread();
    freeAppData();
    freeDB(&db_conn_data);
    data_initialized = 0;
}

void freeApp() {
    freeData();
    freeSocketFd(&udp_fd);
    freeDB(&db_conn_settings);
    freePid(&pid_file, &proc_id, pid_path);
}

void exit_nicely() {
    freeApp();
    puts("\nBye...");
    exit(EXIT_SUCCESS);
}

void exit_nicely_e(char *s) {
    fprintf(stderr, "%s", s);
    freeApp();
    exit(EXIT_FAILURE);
}

int main(int argc, char** argv) {
#ifndef MODE_DEBUG
    daemon(0, 0);
#endif
    conSig(&exit_nicely);
    if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
        perror("main: memory locking failed");
    }
#ifndef MODE_DEBUG
    setPriorityMax(SCHED_FIFO);
#endif
    while (1) {
        switch (app_state) {
            case APP_INIT:puts("APP_INIT");
                initApp();
                app_state = APP_INIT_DATA;
                break;
            case APP_INIT_DATA:puts("APP_INIT_DATA");
                initData();
                app_state = APP_RUN;
                break;
            case APP_RUN:
                serverRun(&app_state, data_initialized);
                break;
            case APP_STOP:puts("APP_STOP");
                freeData();
                app_state = APP_RUN;
                break;
            case APP_RESET:puts("APP_RESET");
                freeApp();
                app_state = APP_INIT;
                break;
            case APP_EXIT:puts("APP_EXIT");
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

