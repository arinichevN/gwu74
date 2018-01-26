
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

int bufCatPinIn(const Pin *item, ACPResponse *response) {
    if (item->mode == DIO_MODE_IN) {
        return acp_responseFTSCat(item->net_id, (float) item->value, item->tm, item->value_state, response);
    }
    return 0;
}

int bufCatPinOut(const Pin *item, ACPResponse *response) {
    if (item->mode == DIO_MODE_OUT) {
        struct timespec tm = getCurrentTime();
        return acp_responseITSCat(item->net_id, item->out, tm, 1, response);
    }
    return 0;
}

int bufCatDataInit(const Pin *item, ACPResponse *response) {
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "%d" ACP_DELIMITER_COLUMN_STR "%s" ACP_DELIMITER_COLUMN_STR "%s" ACP_DELIMITER_COLUMN_STR "%ld" ACP_DELIMITER_COLUMN_STR "%ld" ACP_DELIMITER_COLUMN_STR "%ld" ACP_DELIMITER_COLUMN_STR "%d" ACP_DELIMITER_COLUMN_STR "%d" ACP_DELIMITER_ROW_STR,
            item->net_id,
            getPinModeStr(item->mode),
            getPinPUDStr(item->pud),
            item->pwm.period.tv_sec,
            item->pwm.period.tv_nsec,
            item->secure_out.timeout.tv_sec,
            item->secure_out.duty_cycle,
            item->secure_out.enable
            );
    return acp_responseStrCat(response, q);
}

void setPinOut(Pin *item, int value) {
    if (item->out == value) {
        return;
    }
    setOut(item, value);
    item->out = value;
}

void setPinOutput(Pin *item, int value) {
    if (item->mode == DIO_MODE_OUT) {
        item->out_pwm = 0;
        setPinOut(item, value);
#ifdef MODE_DEBUG
        printf("%s(): pin %d output %d\n", __FUNCTION__, item->net_id, value);
#endif
    }
}

void setPinDutyCyclePWM(Pin *item, int value) {
    if (item->mode == DIO_MODE_OUT) {
        item->out_pwm = 1;
        item->duty_cycle = value;
#ifdef MODE_DEBUG
        printf("%s(): pin %d pwm %d\n", __FUNCTION__, item->net_id, value);
#endif
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

int needSecure(DOSecure *item) {
    if (item->enable && !item->done) {
        if (ton_ts(item->timeout, &item->tmr)) {
            return 1;
        }
    }
    return 0;
}

void resetSecure(DOSecure *item) {
    item->done = 0;
    item->tmr.ready = 0;
}

void doneSecure(DOSecure *item) {
    item->done = 1;
}
