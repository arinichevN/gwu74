
#include "device/main.h"

//FUN_LIST_GET_BY_ID(Device)

FUN_LIST_GET_BY(id, Device)

FUN_LIST_GET_BY(net_id, Pin)

FUN_LOCK(Pin)

FUN_LOCK(Device)

FUN_TRYLOCK(Pin)

FUN_TRYLOCK(Device)

FUN_UNLOCK(Pin)

FUN_UNLOCK(Device)

int lockPD(Pin *item) {
    if (lockPin(item)) {
        if (lockDevice(item->device)) {
            return 1;
        } else {
            unlockPin(item);
        }
    }
    return 0;
}

int tryLockPD(Pin *item) {
    if (tryLockPin(item)) {
        if (tryLockDevice(item->device)) {
            return 1;
        } else {
            unlockPin(item);
        }
    }
    return 0;
}

void unlockPD(Pin *item) {
    unlockPin(item);
    unlockDevice(item->device);
}

int lockPinAll(PinList *list) {
    size_t i;
    int done = 1;
    for (i = 0; i < list->length; i++) {
        done = done && lockPin(&list->item[i]);
        if (!done) {
            break;
        }
    }
    return done;
}

int lockDeviceAll(DeviceList *list) {
    size_t i;
    int done = 1;
    for (i = 0; i < list->length; i++) {
        done = done && lockDevice(&list->item[i]);
        if (!done) {
            break;
        }
    }
    return done;
}

void unlockPinAll(PinList *list) {
    size_t i;
    for (i = 0; i < list->length; i++) {
        unlockPin(&list->item[i]);
    }
}

void unlockDeviceAll(DeviceList *list) {
    size_t i;
    for (i = 0; i < list->length; i++) {
        unlockDevice(&list->item[i]);
    }
}

int lockPDAll(DeviceList *dl, PinList *pl) {
    if (lockPinAll(pl)) {
        if (lockDeviceAll(dl)) {
            return 1;
        } else {
            unlockPinAll(pl);
            unlockDeviceAll(dl);
        }
    } else {
        unlockPinAll(pl);
    }
    return 0;
}

void unlockPDAll(DeviceList *dl, PinList *pl) {
    unlockPinAll(pl);
    unlockDeviceAll(dl);
}

void app_writeDeviceList(DeviceList *dl) {
    if (lockDeviceAll(dl)) {
        writeDeviceList(dl);
        unlockDeviceAll(dl);
    }
}

int getModeByStr(const char *str) {
    if (strcmp(str, DIO_MODE_IN_STR) == 0) {
        return DIO_MODE_IN;
    } else if (strcmp(str, DIO_MODE_OUT_STR) == 0) {
        return DIO_MODE_OUT;
    } else {
        return -1;
    }
}

int getPUDByStr(const char *str) {
    if (strcmp(str, DIO_PUD_OFF_STR) == 0) {
        return DIO_PUD_OFF;
    } else if (strcmp(str, DIO_PUD_UP_STR) == 0) {
        return DIO_PUD_UP;
    } else if (strcmp(str, DIO_PUD_DOWN_STR) == 0) {
        return DIO_PUD_DOWN;
    } else {
        return -1;
    }
}

char * getPinModeStr(char in) {
    switch (in) {
        case DIO_MODE_IN:
            return "IN";
        case DIO_MODE_OUT:
            return "OUT";
    }
    return "?";
}

char * getCmdStrLocal(char in) {
    switch (in) {
        case ACP_CMD_GET_INT:
            return "ACP_CMD_GET_INT";
        case ACP_CMD_GWU74_GET_OUT:
            return "ACP_CMD_GWU74_GET_OUT";
        case ACP_CMD_GWU74_GET_DATA:
            return "ACP_CMD_GWU74_GET_DATA";
        case ACP_CMD_SET_INT:
            return "ACP_CMD_SET_INT";
        case ACP_CMD_SET_DUTY_CYCLE_PWM:
            return "ACP_CMD_SET_DUTY_CYCLE_PWM";
        case ACP_CMD_SET_PWM_PERIOD:
            return "ACP_CMD_SET_PWM_PERIOD";
        case ACP_CMD_GWU74_SET_RSL:
            return "ACP_CMD_GWU74_SET_RSL";
    }
    return "?";
}

char * getPinPUDStr(char in) {
    switch (in) {
        case DIO_PUD_OFF:
            return "OFF";
        case DIO_PUD_UP:
            return "UP";
        case DIO_PUD_DOWN:
            return "DOWN";
    }
    return "?";
}

void savePin(Pin *item, const char *db_path) {
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "update pin set pwm_period_sec=%ld, pwm_period_nsec=%ld, rsl=%u where net_id=%d", item->pwm.period.tv_sec, item->pwm.period.tv_nsec, item->pwm.rsl, item->net_id);
    sqlite3 *db;
    if (!db_open(db_path, &db)) {
        return;
    }
    if (!db_exec(db, q, 0, 0)) {
#ifdef MODE_DEBUG
        fprintf(stderr, "savePin: query failed: %s\n", q);
#endif
    }
    sqlite3_close(db);
#ifdef MODE_DEBUG
    printf("savePin: done: %s\n", q);
#endif
}

int bufCatPinIn(const Pin *item, char *buf, size_t buf_size) {
    if (item->mode == DIO_MODE_IN) {
        char q[LINE_SIZE];
        snprintf(q, sizeof q, "%d" ACP_DELIMITER_COLUMN_STR "%d" ACP_DELIMITER_ROW_STR, item->net_id, item->value);
        if (bufCat(buf, q, buf_size) == NULL) {
            return 0;
        }
    }
    return 1;
}

int bufCatPinOut(const Pin *item, char *buf, size_t buf_size) {
    if (item->mode == DIO_MODE_OUT) {
        char q[LINE_SIZE];
        snprintf(q, sizeof q, "%d" ACP_DELIMITER_COLUMN_STR "%d" ACP_DELIMITER_ROW_STR, item->net_id, item->out);
        if (bufCat(buf, q, buf_size) == NULL) {
            return 0;
        }
    }
    return 1;
}

int bufCatData(const Pin *item, char *buf, size_t buf_size) {
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "%d" ACP_DELIMITER_COLUMN_STR "%s" ACP_DELIMITER_COLUMN_STR "%s" ACP_DELIMITER_COLUMN_STR "%ld" ACP_DELIMITER_COLUMN_STR "%ld" ACP_DELIMITER_ROW_STR,
            item->net_id,
            getPinModeStr(item->mode),
            getPinPUDStr(item->pud),
            item->pwm.period.tv_sec,
            item->pwm.period.tv_nsec
            );
    if (bufCat(buf, q, buf_size) == NULL) {
        return 0;
    }
    return 1;
}

void setPinOut(Pin *item, int value) {
    if (item->out == value) {
        return;
    }
    setOut(item, value);
    item->out = value;
}

void setPinOutput(Pin *item, int value) {
#ifdef MODE_DEBUG
    puts("setPinOutput");
#endif
    if (lockPD(item)) {
        if (item->mode == DIO_MODE_OUT) {
            item->out_pwm = 0;
            setPinOut(item, value);
        }
        unlockPD(item);
    }
}

void setPinDutyCyclePWM(Pin *item, int value) {
#ifdef MODE_DEBUG
    puts("setPinDutyCyclePWM");
#endif
    if (lockPD(item)) {
        if (item->mode == DIO_MODE_OUT) {
            item->out_pwm = 1;
            item->duty_cycle = value;
        }
        unlockPD(item);
    }
}

void setPinPeriodPWM(Pin *item, int value, const char *db_data_path) {
#ifdef MODE_DEBUG
    puts("setPinPeriodPWM");
#endif
    if (lockPin(item)) {
        usec2timespec(value, &item->pwm.period)
        savePin(item, db_data_path);
        unlockPin(item);
    }
}

void setPinRslPWM(Pin *item, int value, const char *db_data_path) {
#ifdef MODE_DEBUG
    puts("setPinRslPWM");
#endif
    if (lockPin(item)) {
        if (value > 0) {
            item->pwm.rsl = value;
            savePin(item, db_data_path);
        }
        unlockPin(item);
    }
}

int sendStrPack(char qnf, char *cmd) {
    extern Peer peer_client;
    return acp_sendStrPack(qnf, cmd, &peer_client);
}

int sendBufPack(char *buf, char qnf, char *cmd_str) {
    extern Peer peer_client;
    return acp_sendBufPack(buf, qnf, cmd_str, &peer_client);
}

void sendStr(const char *s, uint8_t *crc) {
    acp_sendStr(s, crc, &peer_client);
}

void sendFooter(int8_t crc) {
    acp_sendFooter(crc, &peer_client);
}

void waitThread_ctl(char cmd) {
    thread_cmd = cmd;
    pthread_join(thread, NULL);
}