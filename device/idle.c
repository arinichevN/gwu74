
#include "main.h"

void idle_setMode(Pin *pin, int mode) {
    ;
}

void idle_setPUD(Pin *pin, int pud) {
    ;
}

void idle_setOut(Pin *pin, int value) {
    ;
}

void idle_getIn(Pin *pin) {
    ;
}

void idle_writeDeviceList(DeviceList *list) {
    ;
}

void idle_readDeviceList(DeviceList *list, PinList *pl) {
    ;
}

int idle_checkDevPin(DeviceList *dl, PinList *pl) {
    for (int i = 0; i < pl->length; i++) {
        if (pl->item[i].mode != DIO_MODE_IN && pl->item[i].mode != DIO_MODE_OUT) {
            fprintf(stderr, "%s(): bad mode where net_id = %d\n",F, pl->item[i].net_id);
            return 0;
        }
        if (pl->item[i].pud != DIO_PUD_OFF && pl->item[i].pud != DIO_PUD_UP && pl->item[i].pud != DIO_PUD_DOWN) {
            fprintf(stderr, "%s(): bad PUD where net_id = %d\n",F, pl->item[i].net_id);
            return 0;
        }
        if (pl->item[i].pwm.period.tv_sec < 0 || pl->item[i].pwm.period.tv_nsec < 0) {
            fprintf(stderr, "%s(): bad pwm_period where net_id = %d\n",F, pl->item[i].net_id);
            return 0;
        }
        if (pl->item[i].pwm.rsl < 0) {
            fprintf(stderr, "%s(): bad rsl where net_id = %d\n",F, pl->item[i].net_id);
            return 0;
        }
    }
    for (int i = 0; i < pl->length; i++) {
        for (int j = i + 1; j < pl->length; j++) {
            if (pl->item[i].net_id == pl->item[j].net_id) {
                fprintf(stderr, "%s(): net_id is not unique where net_id = %d\n",F, pl->item[i].net_id);
                return 0;
            }
        }
    }


    return 1;
}

void idle_setPtf() {
    setMode = idle_setMode;
    setPUD = idle_setPUD;
    setOut = idle_setOut;
    getIn = idle_getIn;
    writeDeviceList = idle_writeDeviceList;
    readDeviceList = idle_readDeviceList;
}

int idle_initDevPin(DeviceList *dl, PinList *pl, const char *db_path) {
    if (!initDevPin(dl, pl, IDLE_MAX_DEV_NUM, IDLE_MAX_PIN_NUM, db_path)) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): failed\n", F);
#endif
        return 0;
    }
    for (int i = 0; i < pl->length; i++) {
        pl->item[i].device = NULL;
    }
    if (!idle_checkDevPin(dl, pl)) {
        return 0;
    }
    idle_setPtf();
    return 1;
}