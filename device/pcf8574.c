#include "pcf8574.h"

void pcf8574_setMode(Pin *pin, int mode) {
    int bit, old;

    bit = 1 << (pin->id_dev & 7);

    old = pin->device->old_data1;
    switch (mode) {
        case DIO_MODE_OUT:
            old &= (~bit); // Write bit to 0
            break;
        case DIO_MODE_IN:
            old |= bit; // Write bit to 1 ground
            break;
        default:
            return;
    }

    pin->device->old_data1 = old;
}

void pcf8574_setPUD(Pin *pin, int pud) {
    ;
}

//call writeDeviceList() function after this function and then data will be written to chip

void pcf8574_setOut(Pin *pin, int value) {
    int bit;
    bit = 1 << (pin->id_dev & 7);
    switch (value) {
        case DIO_LOW:
            pin->device->new_data1 &= (~bit);
            break;
        case DIO_HIGH:
            pin->device->new_data1 |= bit;
            break;
        default:
            return;
    }
}


//call readDeviceList() after this function and then pin->value will be updated

void pcf8574_getIn(Pin *pin) {
    pin->device->read1 = 1;
}

void pcf8574_writeDeviceList(DeviceList *list) {
    int i;
    for (i = 0; i < list->length; i++) {
        if (list->item[i].new_data1 != list->item[i].old_data1) {
            I2CWrite(list->item[i].fd_i2c, list->item[i].new_data1);
            list->item[i].old_data1 = list->item[i].new_data1;
        }
    }
}

void pcf8574_readDeviceList(DeviceList *list, PinList *pl) {
    int i, j;
    for (i = 0; i < list->length; i++) {
        if (list->item[i].read1) {
            int value = I2CRead(list->item[i].fd_i2c);
            for (j = 0; j < pl->length; j++) {
                if (pl->item[j].device->id == list->item[i].id && pl->item[j].mode == DIO_MODE_IN) {
                    int mask = 1 << (pl->item[j].id_dev & 7);
                    if ((value & mask) == 0) {
                        pl->item[j].value = DIO_LOW;
                    } else {
                        pl->item[j].value = DIO_HIGH;
                    }
                }
            }
            list->item[i].read1 = 0;
        }
    }
}


int pcf8574_checkData(DeviceList *dl, PinList *pl) {
    size_t i, j;
    for (i = 0; i < pl->length; i++) {
        if (pl->item[i].id_dev < 0 || pl->item[i].id_dev >= PCF8574_DEVICE_PIN_NUM) {
            fprintf(stderr, "ERROR: checkData: bad id_within_device where net_id = %d\n", pl->item[i].net_id);
            return 0;
        }
        if (pl->item[i].device == NULL) {
            fprintf(stderr, "ERROR: checkData: no device assigned to pin where net_id = %d\n", pl->item[i].net_id);
            return 0;
        }
        if (pl->item[i].mode != DIO_MODE_IN && pl->item[i].mode != DIO_MODE_OUT) {
            fprintf(stderr, "ERROR: checkData: bad mode where net_id = %d\n", pl->item[i].net_id);
            return 0;
        }
        if (pl->item[i].pud != DIO_PUD_UP) {
            fprintf(stderr, "ERROR: checkData: bad PUD where net_id = %d (up expected)\n", pl->item[i].net_id);
            return 0;
        }
        if (pl->item[i].pwm.period.tv_sec < 0 || pl->item[i].pwm.period.tv_nsec < 0) {
            fprintf(stderr, "ERROR: checkData: bad pwm_period where net_id = %d\n", pl->item[i].net_id);
            return 0;
        }
    }
    for (i = 0; i < pl->length; i++) {
        for (j = i + 1; j < pl->length; j++) {
            if (pl->item[i].net_id == pl->item[j].net_id) {
                fprintf(stderr, "ERROR: checkData: net_id is not unique where net_id = %d\n", pl->item[i].net_id);
                return 0;
            }
        }
    }
    for (i = 0; i < pl->length; i++) {
        for (j = i + 1; j < pl->length; j++) {
            if (pl->item[i].id_dev == pl->item[j].id_dev && pl->item[i].device->id == pl->item[j].device->id) {
                fprintf(stderr, "ERROR: checkData: id_within_device is not unique where net_id = %d\n", pl->item[i].net_id);
                return 0;
            }
        }
    }
    return 1;
}

void pcf8574_setPtf() {
    setMode = pcf8574_setMode;
    setPUD = pcf8574_setPUD;
    setOut = pcf8574_setOut;
    getIn = pcf8574_getIn;
    writeDeviceList = pcf8574_writeDeviceList;
    readDeviceList = pcf8574_readDeviceList;
}

int pcf8574_initDevPin(DeviceList *dl, PinList *pl, PGconn *db_conn, char *app_class, char *i2c_path ) {
    PGresult *r;
    char q[LINE_SIZE];
    size_t i;
    snprintf(q, sizeof q, "select id, addr_i2c from " APP_NAME_STR ".device where app_class='%s' limit %d", app_class, PCF8574_MAX_DEV_NUM);
    if ((r = dbGetDataT(db_conn, q, q)) == NULL) {
        return 0;
    }
    dl->length = PQntuples(r);
    if (dl->length > 0) {
        dl->item = (Device *) malloc(dl->length * sizeof *(dl->item));
        if (dl->item == NULL) {
            fputs("ERROR: initDevPin: failed to allocate memory for devices\n", stderr);
            PQclear(r);
            return 0;
        }
        for (i = 0; i < dl->length; i++) {
            memset(&dl->item[i], 0,sizeof dl->item[i]);
            dl->item[i].id = atoi(PQgetvalue(r, i, 0));
            dl->item[i].fd_i2c = I2COpen(i2c_path, atoi(PQgetvalue(r, i, 1)));
            if (dl->item[i].fd_i2c == -1) {
                fputs("ERROR: initDevPin: I2COpen\n", stderr);
                PQclear(r);
                return 0;
            }
            dl->item[i].old_data1 = I2CRead(dl->item[i].fd_i2c);
            dl->item[i].new_data1 = dl->item[i].old_data1;
            if (dl->item[i].old_data1 == -1) {
                fputs("ERROR: initDevPin: I2CRead\n", stderr);
                PQclear(r);
                return 0;
            }
        }
    }
    PQclear(r);
    snprintf(q, sizeof q, "select net_id, device_id, id_within_device, mode, pud, pwm_period_sec, pwm_period_nsec from " APP_NAME_STR ".pin where app_class='%s' limit %d", app_class, PCF8574_MAX_PIN_NUM);
    if ((r = dbGetDataT(db_conn, q, q)) == NULL) {
        return 0;
    }
    pl->length = PQntuples(r);
    if (pl->length > 0) {
        pl->item = (Pin *) malloc(pl->length * sizeof *(pl->item));
        if (pl->item == NULL) {
            fputs("ERROR: initDevPin: failed to allocate memory for pins\n", stderr);
            PQclear(r);
            return 0;
        }
        for (i = 0; i < pl->length; i++) {
            memset(&pl->item[i], 0,sizeof pl->item[i]);
            pl->item[i].net_id = atoi(PQgetvalue(r, i, 0));
            pl->item[i].device = getDeviceBy_id(atoi(PQgetvalue(r, i, 1)), dl);
            pl->item[i].id_dev = atoi(PQgetvalue(r, i, 2));
            pl->item[i].mode = getModeByStr(PQgetvalue(r, i, 3));
            pl->item[i].pud = getPUDByStr(PQgetvalue(r, i, 4));
            pl->item[i].pwm.period.tv_sec = atoi(PQgetvalue(r, i, 5));
            pl->item[i].pwm.period.tv_nsec = atoi(PQgetvalue(r, i, 6));
        }
    }
    PQclear(r);
    if (!pcf8574_checkData(dl, pl)) {
        return 0;
    }
    pcf8574_setPtf();
    return 1;
}
