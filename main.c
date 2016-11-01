/*
 * UDP gateway for PCF8574
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
int has_input_p = 0; //there are no pins configured as input
int has_output_p = 0; //there are no pins configured as output

Peer peer_client = {.fd = &udp_fd, .addr_size = sizeof peer_client.addr};

char db_conninfo_settings[LINE_SIZE];
char db_conninfo_data[LINE_SIZE];
PGconn *db_conn_settings = NULL;
PGconn *db_conn_data = NULL;
PGconn **db_connp_data = NULL;
int db_init_data = 0;
ThreadData thread_data = {.cmd = ACP_CMD_APP_NO, .on = 0, .i1l =
    {NULL, 0}, .i2l =
    {NULL, 0}};
DeviceList device_list = {NULL, 0};
PinList pin_list = {NULL, 0};
int pb[MAX_PIN_NUM];

#include "pcf8574.c"
#include "util.c"
#include "print.c"

int readSettings() {
    PGresult *r;
    char q[LINE_SIZE];
    memset(pid_path, 0, sizeof pid_path);
    memset(i2c_path, 0, sizeof i2c_path);
    memset(db_conninfo_data, 0, sizeof db_conninfo_data);

    snprintf(q, sizeof q, "select db_public from %s.config where app_class='%s'", APP_NAME, app_class);
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

    snprintf(q, sizeof q, "select udp_port, i2c_path, pid_path, udp_buf_size, db_data, cycle_duration_us from %s.config where app_class='%s'", APP_NAME, app_class);
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

int getModeByStr(const char *str) {
    if (strcmp(str, MODE_IN_STR) == 0) {
        return MODE_IN;
    } else if (strcmp(str, MODE_OUT_STR) == 0) {
        return MODE_OUT_CMN;
    } else {
        return -1;
    }
}

int initAppData(DeviceList *dl, PinList *pl) {
    PGresult *r;
    char q[LINE_SIZE];
    size_t i, j;
    snprintf(q, sizeof q, "select addr from %s.device where app_class='%s' order by addr", APP_NAME, app_class);
    if ((r = dbGetDataT(*db_connp_data, q, "initAppData: select pin: ")) == NULL) {
        return 0;
    }
    dl->length = PQntuples(r);
    if (dl->length > 0) {
        dl->device = (Device *) malloc(dl->length * sizeof *(dl->device));
        if (dl->device == NULL) {
            dl->length = 0;
            fputs("initAppData: failed to allocate memory for devices\n", stderr);
            return 0;
        }
        pl->length = dl->length * DEVICE_PIN_NUM;
        pl->item = (Pin *) malloc(pl->length * sizeof *(pl->item));
        if (pl->item == NULL) {
            pl->length = 0;
            fputs("initAppData: failed to allocate memory for pins\n", stderr);
            return 0;
        }
        for (i = 0; i < dl->length; i++) {
            dl->device[i].addr = atoi(PQgetvalue(r, i, 0));
#ifdef P_A20
            dl->device[i].fd = I2COpen(i2c_path, dl->device[i].addr);
#endif
            if (dl->device[i].fd == -1) {
                fputs("initAppData: I2COpen\n", stderr);
                PQclear(r);
                return 0;
            }
#ifdef P_A20
            dl->device[i].old_data = I2CRead(dl->device[i].fd);
#endif
            dl->device[i].new_data = dl->device[i].old_data;
            if (dl->device[i].old_data == -1) {
                fputs("initAppData: I2CRead\n", stderr);
                PQclear(r);
                return 0;
            }
            for (j = 0; j < DEVICE_PIN_NUM; j++) {
                timespecclear(&pl->item[j].pwm.period);
                size_t k = i * DEVICE_PIN_NUM + j;
                pl->item[k].id = k;
                pl->item[k].net_id = -1;
                pl->item[k].id_dev = j;
                pl->item[k].device = &dl->device[i];
                pl->item[k].mode = MODE_OUT_CMN;
                pl->item[k].duty_cycle = 0;
                pl->item[k].pwm.initialized = 0;
                pl->item[k].pwm.period.tv_sec = PWM_PERIOD_DEFAULT;
                pl->item[k].value = HIGH;
                pl->item[k].mode_sv = pl->item[k].mode;
                pl->item[k].pwm_period_sv = pl->item[k].pwm.period;
            }
        }
        PQclear(r);
        //read pin table and overwrite some pin settings
        snprintf(q, sizeof q, "select id, mode, pwm_period from %s.pin where app_class='%s'", APP_NAME, app_class);
        if ((r = dbGetDataT(*db_connp_data, q, "initAppData: select pin: ")) == NULL) {
            return 0;
        }
        int n = PQntuples(r);
        if (n > 0) {
            int mode, pwm_period, pin_id;
            for (i = 0; i < n; i++) {
                pin_id = atoi(PQgetvalue(r, i, 0));
                mode = getModeByStr(PQgetvalue(r, i, 1));
                pwm_period = atoi(PQgetvalue(r, i, 2));
                Pin *p = getPinById(pin_id, pl);
                if (p != NULL) {
                    p->mode = mode;
                    p->pwm.period.tv_sec = pwm_period;
                }
            }
            PQclear(r);
        }

        //net_id assignment
        snprintf(q, sizeof q, "select id, pin_id from %s.id_pin_group where app_class='%s'", APP_NAME, app_class);
        if ((r = dbGetDataT(*db_connp_data, q, "initAppData: select from id_pin_group: ")) == NULL) {
            return 0;
        }
        n = PQntuples(r);
        if (n > 0) {
            for (i = 0; i < n; i++) {
                int id = atoi(PQgetvalue(r, i, 0));
                int pin_id = atoi(PQgetvalue(r, i, 1));
                Pin *p;
                if (PQgetisnull(r, i, 1)) {
                    p = NULL;
                } else {
                    p = getPinById(pin_id, pl);
                }
                if (p != NULL && id >= 0) {
                    p->net_id = id;
                }
            }
        }
        PQclear(r);
        //updating utility variables
        for (i = 0; i < pl->length; i++) {
            if (has_input_p == 0 && pl->item[i].mode == MODE_IN) {
                has_input_p = 1;
            }
            if (has_output_p == 0 && (pl->item[i].mode == MODE_OUT_CMN || pl->item[i].mode == MODE_OUT_PWM)) {
                has_output_p = 1;
            }
        }
    }
    return 1;
}

int checkAppData(DeviceList *dl, PinList *pl) {
    size_t i;

    for (i = 0; i < pl->length; i++) {
        if (!(pl->item[i].mode == MODE_IN || pl->item[i].mode == MODE_OUT_CMN || pl->item[i].mode == MODE_OUT_PWM)) {
            fprintf(stderr, "checkAppData: check pin table: bad mode (%d) for pin with id=%d\n", pl->item[i].mode, i);
            return 0;
        }
        if (pl->item[i].pwm.period.tv_sec < 0) {
            fprintf(stderr, "checkAppData: check pin table: bad pwm_period (%ld) for pin with id=%d\n", pl->item[i].pwm.period.tv_sec, i);
            return 0;
        }
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
        case ACP_CMD_PCF8574_GET_DATA:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                    break;
                case ACP_QUANTIFIER_SPECIFIC:
                    acp_parsePackI1(buf_in, &thread_data.i1l, pin_list.length);
                    if (thread_data.i1l.length <= 0) {
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
                    acp_parsePackI1(buf_in, &thread_data.i1l, pin_list.length);
                    if (thread_data.i1l.length <= 0) {
                        return;
                    }
                    break;
                case ACP_QUANTIFIER_SPECIFIC:
                    acp_parsePackI2(buf_in, &thread_data.i2l, pin_list.length);
                    if (thread_data.i2l.length <= 0) {
                        return;
                    }
                    break;
            }
            break;
        default:
            return;
    }

    switch (buf_in[1]) {
        case ACP_CMD_PCF8574_GET_DATA:
            break;
        case ACP_CMD_GET_INT:
        case ACP_CMD_SET_INT:
        case ACP_CMD_SET_DUTY_CYCLE_PWM:
        case ACP_CMD_SET_PWM_PERIOD:
            if (thread_data.on) {
                waitThreadCmd(&thread_data.cmd, &thread_data.qfr, buf_in);
            }
            break;
    }

    switch (buf_in[1]) {
        case ACP_CMD_GET_INT:
        case ACP_CMD_PCF8574_GET_DATA:
            break;
        case ACP_CMD_SET_INT:
        case ACP_CMD_SET_DUTY_CYCLE_PWM:
            return;
        case ACP_CMD_SET_PWM_PERIOD:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                    for (i = 0; i < pin_list.length; i++) {
                        savePin(&pin_list.item[i]);
                    }
                    break;
                case ACP_QUANTIFIER_SPECIFIC:
                    for (i = 0; i < thread_data.i2l.length; i++) {
                        Pin *p = getPinByNetId(thread_data.i2l.item[i].p0, &pin_list);
                        if (p != NULL) {
                            savePin(p);
                        }
                    }
                    break;
            }
            return;
    }

    switch (buf_in[1]) {
        case ACP_CMD_GET_INT:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                    for (i = 0; i < pin_list.length; i++) {
                        if (!bufCatPinIn(&pin_list.item[i], buf_out, udp_buf_size)) {
                            sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_BUF_OVERFLOW);
                            return;
                        }
                    }
                    break;
                case ACP_QUANTIFIER_SPECIFIC:
                    for (i = 0; i < thread_data.i1l.length; i++) {
                        Pin *p = getPinByNetId(thread_data.i1l.item[i], &pin_list);
                        if (p != NULL) {
                            if (!bufCatPinIn(p, buf_out, udp_buf_size)) {
                                sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_BUF_OVERFLOW);
                                return;
                            }
                        }
                    }
                    break;
            }
            break;
        case ACP_CMD_PCF8574_GET_DATA:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                    for (i = 0; i < pin_list.length; i++) {
                        if (pin_list.item[i].net_id != -1) {
                            if (!bufCatDataPin(&pin_list.item[i], buf_out, udp_buf_size)) {
                                sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_BUF_OVERFLOW);
                                return;
                            }
                        }
                    }
                    break;
                case ACP_QUANTIFIER_SPECIFIC:
                    for (i = 0; i < thread_data.i1l.length; i++) {
                        Pin *p = getPinByNetId(thread_data.i1l.item[i], &pin_list);
                        if (p != NULL) {
                            if (!bufCatDataPin(p, buf_out, udp_buf_size)) {
                                sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_BUF_OVERFLOW);
                                return;
                            }
                        }
                    }
                    break;
            }
            break;
    }

    switch (buf_in[1]) {
        case ACP_CMD_GET_INT:
        case ACP_CMD_PCF8574_GET_DATA:
            if (!sendBufPack(buf_out, ACP_QUANTIFIER_SPECIFIC, ACP_RESP_REQUEST_SUCCEEDED)) {
                sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_BUF_OVERFLOW);
                return;
            }
            return;
    }

}

void writeDevice(DeviceList *list) {
    size_t i;
    for (i = 0; i < list->length; i++) {
        if (list->device[i].old_data != list->device[i].new_data) {
            pcf8574_write(&list->device[i]);
        }
    }
}

void updatePinMode(PinList *list) {
    size_t i;
    for (i = 0; i < list->length; i++) {
        switch (list->item[i].mode) {
            case MODE_IN:
                pcf8574_updatenew(list->item[i].device, list->item[i].id_dev, HIGH); //3.3V
                break;
            case MODE_OUT_CMN:
            case MODE_OUT_PWM:
            case MODE_OUT_PM:
            default:
                pcf8574_updatenew(list->item[i].device, list->item[i].id_dev, HIGH);
                break;
        }
    }
}

void updateOutSafe(PinList *list) {
    size_t i;
    for (i = 0; i < list->length; i++) {
        switch (list->item[i].mode) {
            case MODE_IN:
                break;
            case MODE_OUT_CMN:
            case MODE_OUT_PWM:
            case MODE_OUT_PM:
            default:
                pcf8574_updatenew(list->item[i].device, list->item[i].id_dev, HIGH);
                break;
        }
    }
}

void *threadFunction(void *arg) {
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
    updatePinMode(&pin_list);
    writeDevice(&device_list);
    while (1) {
        size_t i;
        struct timespec t1;
        clock_gettime(LIB_CLOCK, &t1);
        for (i = 0; i < pin_list.length; i++) {
            if (pin_list.item[i].net_id == -1) {//skip pins with no net_id assigned
                break;
            }
            if (pin_list.item[i].mode == MODE_OUT_PWM) {
                int v = pwmctl(&pin_list.item[i].pwm, pin_list.item[i].duty_cycle);
                pcf8574_updatenew(pin_list.item[i].device, pin_list.item[i].id_dev, v);
            }
        }

        switch (data->cmd) {
            case ACP_CMD_GET_INT:
                if (1) {
                    Device * devp[device_list.length];
                    DevicePList dev_plist = {devp, 0}; //devices we should read from
                    dev_plist.length = 0;
                    switch (data->qfr) {
                        case ACP_QUANTIFIER_BROADCAST:
                            for (i = 0; i < pin_list.length; i++) {
                                if (pin_list.item[i].net_id != -1 && pin_list.item[i].mode == MODE_IN) {
                                    devicePListPushUnique(&dev_plist, pin_list.item[i].device);
                                }
                            }
                            break;
                        case ACP_QUANTIFIER_SPECIFIC:
                            for (i = 0; i < data->i1l.length; i++) {
                                Pin *p = getPinByNetId(data->i1l.item[i], &pin_list);
                                if (p != NULL) {
                                    if (p->mode == MODE_IN) {
                                        devicePListPushUnique(&dev_plist, p->device);
                                    }
                                }
                            }
                            break;
                    }
                    pcf8574ReadUpdatePinValue(&dev_plist, &pin_list);
                }
                break;
            case ACP_CMD_SET_INT:
                switch (data->qfr) {
                    case ACP_QUANTIFIER_BROADCAST:
                        for (i = 0; i < pin_list.length; i++) {
                            if (pin_list.item[i].net_id != -1) {
                                setPinOutput(&pin_list.item[i], data->i1l.item[0]);
                            }
                        }
                        break;
                    case ACP_QUANTIFIER_SPECIFIC:
                        for (i = 0; i < data->i2l.length; i++) {
                            Pin *p = getPinByNetId(data->i2l.item[i].p0, &pin_list);
                            if (p != NULL) {
                                setPinOutput(p, data->i2l.item[i].p1);
                            }
                        }
                        break;
                }
                break;
            case ACP_CMD_SET_DUTY_CYCLE_PWM:
                switch (data->qfr) {
                    case ACP_QUANTIFIER_BROADCAST:
                        for (i = 0; i < pin_list.length; i++) {
                            if (pin_list.item[i].net_id != -1) {
                                setPinDutyCyclePWM(&pin_list.item[i], data->i1l.item[0]);
                            }
                        }
                        break;
                    case ACP_QUANTIFIER_SPECIFIC:
                        for (i = 0; i < data->i2l.length; i++) {
                            Pin *p = getPinByNetId(data->i2l.item[i].p0, &pin_list);
                            if (p != NULL) {
                                setPinDutyCyclePWM(p, data->i2l.item[i].p1);
                            }
                        }
                        break;
                }
                break;
            case ACP_CMD_SET_PWM_PERIOD:
                switch (data->qfr) {
                    case ACP_QUANTIFIER_BROADCAST:
                        for (i = 0; i < pin_list.length; i++) {
                            if (pin_list.item[i].net_id != -1) {
                                pin_list.item[i].pwm.period.tv_sec = data->i1l.item[0];
                            }
                        }
                        break;
                    case ACP_QUANTIFIER_SPECIFIC:
                        for (i = 0; i < data->i2l.length; i++) {
                            Pin *p = getPinByNetId(data->i2l.item[i].p0, &pin_list);
                            if (p != NULL) {
                                p->pwm.period.tv_sec = data->i2l.item[i].p1;
                            }
                        }
                        break;
                }
                break;

            case ACP_CMD_APP_STOP:
            case ACP_CMD_APP_RESET:
            case ACP_CMD_APP_EXIT:
                updateOutSafe(&pin_list);
                writeDevice(&device_list);
                data->cmd = ACP_CMD_APP_NO;
                data->on = 0;
                return (EXIT_SUCCESS);
            default:
                break;
        }

        //writing to chips if data changed
        writeDevice(&device_list);

        data->cmd = ACP_CMD_APP_NO; //notify main thread that command has been executed
        sleepRest(data->cycle_duration, t1);

    }
}

int createThread(ThreadData * td, size_t buf_len) {
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
    td->i1l.item = malloc(buf_len * sizeof *(td->i1l.item));
    if (td->i1l.item == NULL) {
        perror("createThreads: memory allocation for i1l failed");
        return 0;
    }
    td->i2l.item = malloc(buf_len * sizeof *(td->i2l.item));
    if (td->i2l.item == NULL) {
        perror("createThreads: memory allocation for i2l failed");
        return 0;
    }
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
}

void initData() {
    data_initialized = 0;
    if (db_init_data) {
        if (!initDB(&db_conn_data, db_conninfo_data)) {
            return;
        }
        puts("initData: db_data initialized");
    }
    if (!initAppData(&device_list, &pin_list)) {
        puts("initData: initAppData FAILED");
        freeAppData();
        freeDB(&db_conn_data);
        return;
    }
    if (!checkAppData(&device_list, &pin_list)) {
        puts("initData: check FAILED");
        freeAppData();
        freeDB(&db_conn_data);
        return;
    }
    // printDevice(dl);
    // printPin(pl);
    if (!createThread(&thread_data, pin_list.length)) {
        puts("initData: createThread FAILED");
        freeThread();
        freeAppData();
        freeDB(&db_conn_data);
        return;
    }
    data_initialized = 1;
}

void freeThread() {
    if (thread_data.init_success.thread_create) {
        if (thread_data.on) {
            char cmd[2];
            cmd[1] = ACP_CMD_APP_EXIT;
            waitThreadCmd(&thread_data.cmd, &thread_data.qfr, cmd);
        }
        if (pthread_attr_destroy(&thread_data.thread_attr) != 0) {
            perror("freeThread: pthread_attr_destroy");
        }
    } else {
        if (thread_data.init_success.thread_attr_init) {
            if (pthread_attr_destroy(&thread_data.thread_attr) != 0) {
                perror("freeThread: pthread_attr_destroy");
            }
        }
    }
    free(thread_data.i1l.item);
    thread_data.i1l.item = NULL;
    free(thread_data.i2l.item);
    thread_data.i2l.item = NULL;
    thread_data.on = 0;
    thread_data.cmd = ACP_CMD_APP_NO;
}

void freeAppData() {
    free(pin_list.item);
    pin_list.item = NULL;
    pin_list.length = 0;
    free(device_list.device);
    device_list.device = NULL;
    device_list.length = 0;

    has_input_p = 0;
    has_output_p = 0;
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
            case APP_INIT:
                initApp();
                app_state = APP_INIT_DATA;
                break;
            case APP_INIT_DATA:
                initData();
                app_state = APP_RUN;
                break;
            case APP_RUN:
                serverRun(&app_state, data_initialized);
                break;
            case APP_STOP:
                freeData();
                app_state = APP_RUN;
                break;
            case APP_RESET:
                freeApp();
                app_state = APP_INIT;
                break;
            case APP_EXIT:
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

