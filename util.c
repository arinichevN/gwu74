
#include "lib/pwm.h"
#include "main.h"

int devicePListSearch(const DevicePList *list, const Device *item) {
    size_t i;
    for (i = 0; i < list->length; i++) {
        if (list->item[i]->addr == item->addr) {
            return i;
        }
    }
    return -1;
}

void devicePListPush(DevicePList *list, const Device *item) {
    list->item[list->length] = item;
    list->length++;
}

int devicePListPushUnique(DevicePList *list, const Device *item) {
    if (devicePListSearch(list, item) == -1) {
        devicePListPush(list, item);
        return 1;
    }
    return 0;
}

void pcf8574UpdatePinValue(int data, const Device *device, PinList *pin_list) {
    int i;
    for (i = 0; i < pin_list->length; i++) {
        if (pin_list->item[i].device->addr == device->addr && pin_list->item[i].mode == MODE_IN) {
            pin_list->item[i].value = pcf8574_getpinval(data, pin_list->item[i].id_dev);
        }
    }
}

void pcf8574ReadUpdatePinValue(const DevicePList *dev_plist, PinList *pin_list) {
    int i;
    for (i = 0; i < dev_plist->length; i++) {
        int data = pcf8574_read(dev_plist->item[i]);
        pcf8574UpdatePinValue(data, dev_plist->item[i], pin_list);
    }
}

Pin *getPinById(int id, const PinList *list) {
    LIST_GET_BY_ID
}

Pin *getPinByNetId(int net_id, const PinList *list) {
    size_t i;
    for (i = 0; i < list->length; i++) {
        if (net_id == list->item[i].net_id) {
            return &(list->item[i]);
        }
    }
    return NULL;
}

void savePin(Pin *item) {
    if (item->mode != item->mode_sv || timespeccmp(&item->pwm.period, &item->pwm_period_sv, !=)) {
        PGresult *r;
        char q[LINE_SIZE];
        char *mode = "err";
        switch (item->mode) {
            case MODE_IN:
                mode = MODE_IN_STR;
                break;
            case MODE_OUT_CMN:
            case MODE_OUT_PWM:
                mode = MODE_OUT_STR;
                break;
        }
        snprintf(q, sizeof q, "select count(*) from %s.pin where app_class='%s' and id=%d", APP_NAME, app_class, item->id);
        int n = dbGetDataN(*db_connp_data, q, "backupDelProgById: delete from backup: ");
        if (n == 1) {
            snprintf(q, sizeof q, "update %s.pin set mode='%s', pwm_period=%ld where app_class='%s' and id=%d", APP_NAME, mode, item->pwm.period.tv_sec, app_class, item->id);
            if ((r = dbGetDataC(*db_connp_data, q, "backupDelProgById: delete from backup: ")) != NULL) {
                item->mode_sv = item->mode;
                item->pwm_period_sv = item->pwm.period;
                PQclear(r);
            }
        } else if (n == 0) {
            snprintf(q, sizeof q, "insert into %s.pin (app_class, id, mode, pwm_period) values ('%s', %d, '%s', %d)", APP_NAME, app_class, item->id, mode, item->pwm.period.tv_sec);
            if ((r = dbGetDataC(*db_connp_data, q, "backupDelProgById: delete from backup: ")) != NULL) {
                item->mode_sv = item->mode;
                item->pwm_period_sv = item->pwm.period;
                PQclear(r);
            }
        }

    }

}

int bufCatPinIn(const Pin *item, char *buf, size_t buf_size) {
    if (item->net_id != -1 && item->mode == MODE_IN) {
        char q[LINE_SIZE];
        snprintf(q, sizeof q, "%d_%d\n", item->net_id, item->value);
        if (bufCat(buf, q, buf_size) == NULL) {
            return 0;
        }
    }
    return 1;
}

int bufCatData(int net_id, int mode, struct timespec pwm_period, struct timespec pm_busy_time_min, struct timespec pm_idle_time_min, char *buf, size_t buf_size) {
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "%d_%d_%ld_%ld_%ld\n", net_id, mode, pwm_period.tv_sec, pm_busy_time_min.tv_sec, pm_idle_time_min.tv_sec);
    if (bufCat(buf, q, buf_size) == NULL) {
        return 0;
    }
    return 1;
}

int bufCatDataPin(const Pin *item, char *buf, size_t buf_size) {
    struct timespec tm = {-1L, -1L};
    return bufCatData(item->net_id, item->mode, item->pwm.period, tm, tm, buf, buf_size);
}

void setPinOutput(Pin *item, int value) {
    if (item->mode == MODE_OUT_CMN || item->mode == MODE_OUT_PWM) {
        item->mode = MODE_OUT_CMN;
        pcf8574_updatenew(item->device, item->id_dev, value);
    }
}

void setPinDutyCyclePWM(Pin *item, int value) {
    if (item->mode == MODE_OUT_CMN || item->mode == MODE_OUT_PWM) {
        item->mode = MODE_OUT_PWM;
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