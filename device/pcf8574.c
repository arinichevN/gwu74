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

    FORLISTP(list, i) {
        if (list->item[i].new_data1 != list->item[i].old_data1) {
            I2CWrite(list->item[i].i2c_fd, list->item[i].new_data1);
            list->item[i].old_data1 = list->item[i].new_data1;
        }
    }
}

void pcf8574_readDeviceList(DeviceList *list, PinList *pl) {

    FORLISTP(list, i) {
        if (list->item[i].read1) {
            int value = I2CRead(list->item[i].i2c_fd);
            for (size_t j = 0; j < pl->length; j++) {
                if (pl->item[j].device->id == list->item[i].id && pl->item[j].mode == DIO_MODE_IN) {
                    int mask = 1 << (pl->item[j].id_dev & 7);
                    if ((value & mask) == 0) {
                        pl->item[j].value = DIO_LOW;
                    } else {
                        pl->item[j].value = DIO_HIGH;
                    }
                    pl->item[j].tm = getCurrentTime();
                    pl->item[j].value_state = 1;
                }
            }
            list->item[i].read1 = 0;
        }
    }
}

int pcf8574_checkData(DeviceList *dl, PinList *pl) {

    FORLISTP(pl, i) {
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
        if (pl->item[i].pwm.rsl < 0) {
            fprintf(stderr, "ERROR: checkData: bad rsl where net_id = %d\n", pl->item[i].net_id);
            return 0;
        }
    }

    FORLISTP(pl, i) {
        for (size_t j = i + 1; j < pl->length; j++) {
            if (pl->item[i].net_id == pl->item[j].net_id) {
                fprintf(stderr, "ERROR: checkData: net_id is not unique where net_id = %d\n", pl->item[i].net_id);
                return 0;
            }
        }
    }

    FORLISTP(pl, i) {
        for (size_t j = i + 1; j < pl->length; j++) {
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

int pcf8574_initDevPin(DeviceList *dl, PinList *pl, const char *db_path) {
    if (!initDevPin(dl, pl, PCF8574_MAX_DEV_NUM, PCF8574_MAX_PIN_NUM, db_path)) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): failed\n", __FUNCTION__);
#endif
        return 0;
    }
    for (int i = 0; i < dl->length; i++) {
        dl->item[i].i2c_fd = I2COpen(dl->item[i].i2c_path, dl->item[i].i2c_addr);
        if (dl->item[i].i2c_fd == -1) {
            putse("pcf8574_initDevPin: I2COpen failed\n");
            return 0;
        }
        dl->item[i].old_data1 = I2CRead(dl->item[i].i2c_fd);
        if (dl->item[i].old_data1 == -1) {
            putse("ERROR: pcf8574_initDevPin: I2CReadReg8 1\n");
            return 0;
        }
        dl->item[i].new_data1 = dl->item[i].old_data1;
    }

    if (!pcf8574_checkData(dl, pl)) {
        return 0;
    }
    pcf8574_setPtf();
    return 1;
}


