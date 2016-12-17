
#include "device/main.h"

FUN_LIST_GET_BY_ID(Device)

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
    static char *str;
    switch (in) {
        case DIO_MODE_IN:
            str = "IN";
            break;
        case DIO_MODE_OUT:
            str = "OUT";
            break;
        default:
            str = "?";
            break;
    }
    return str;
}

char * getPinPUDStr(char in) {
    static char *str;
    switch (in) {
        case DIO_PUD_OFF:
            str = "OFF";
            break;
        case DIO_PUD_UP:
            str = "UP";
            break;
        case DIO_PUD_DOWN:
            str = "DOWN";
            break;
        default:
            str = "?";
            break;
    }
    return str;
}

void savePin(Pin *item) {
    PGresult *r;
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "update " APP_NAME_STR ".pin set pwm_period_sec=%ld, pwm_period_nsec=%ld where app_class='%s' and net_id=%d", item->pwm.period.tv_sec, item->pwm.period.tv_nsec, app_class, item->net_id);
    if ((r = dbGetDataC(*db_connp_data, q, q)) != NULL) {
        PQclear(r);
    }
}

int bufCatPinIn(const Pin *item, char *buf, size_t buf_size) {
    if (item->mode == DIO_MODE_IN) {
        char q[LINE_SIZE];
        snprintf(q, sizeof q, "%d_%d\n", item->net_id, item->value);
        if (bufCat(buf, q, buf_size) == NULL) {
            return 0;
        }
    }
    return 1;
}

int bufCatPinOut(const Pin *item, char *buf, size_t buf_size) {
    if (item->mode == DIO_MODE_OUT) {
        char q[LINE_SIZE];
        snprintf(q, sizeof q, "%d_%d\n", item->net_id, item->out);
        if (bufCat(buf, q, buf_size) == NULL) {
            return 0;
        }
    }
    return 1;
}

int bufCatData(const Pin *item, char *buf, size_t buf_size) {
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "%d_%s_%s_%ld_%ld\n", 
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

void setPinOutput(Pin *item, int value) {
    if (item->mode == DIO_MODE_OUT) {
        item->out_pwm = 0;
        setOut(item, value);
        item->out=value;
    }
}

void setPinDutyCyclePWM(Pin *item, int value) {
    if (item->mode == DIO_MODE_OUT) {
        item->out_pwm = 1;
        item->duty_cycle = value;
    }
}

int sendStrPack(char qnf, const char *cmd) {
    extern size_t udp_buf_size;
    extern Peer peer_client;
    return acp_sendStrPack(qnf, cmd, udp_buf_size, &peer_client);
}

int sendBufPack(char *buf, char qnf, const char *cmd_str) {
    extern size_t udp_buf_size;
    extern Peer peer_client;
    return acp_sendBufPack(buf, qnf, cmd_str, udp_buf_size, &peer_client);
}

void sendStr(const char *s, uint8_t *crc) {
    acp_sendStr(s, crc, &peer_client);
}

void sendFooter(int8_t crc) {
    acp_sendFooter(crc, &peer_client);
}