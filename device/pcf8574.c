#include "pcf8574.h"

void pcf8574_setMode(Pin *pin, int mode) {
    int bit = 1 << (pin->id_dev & 7);
    int old = pin->device->old_data1;
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
    int bit = 1 << (pin->id_dev & 7);
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
    FORLi {
        if (list->item[i].new_data1 != list->item[i].old_data1) {
            I2CWrite(list->item[i].i2c_fd, list->item[i].new_data1);
            list->item[i].old_data1 = list->item[i].new_data1;
        }
    }
}

void pcf8574_readDeviceList(DeviceList *list, PinList *pl) {
    FORLi {
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
int success=1;
    FORLISTP(pl, i) {
        if (pl->item[i].id_dev < 0 || pl->item[i].id_dev >= PCF8574_DEVICE_PIN_NUM) {
            fprintf(stderr, "%s(): bad id_within_device where net_id = %d\n", F, pl->item[i].net_id);
            success= 0;
        }
        if (pl->item[i].device == NULL) {
            fprintf(stderr, "%s(): no device assigned to pin where net_id = %d\n", F, pl->item[i].net_id);
            success= 0;
        }
        if (pl->item[i].mode != DIO_MODE_IN && pl->item[i].mode != DIO_MODE_OUT) {
            fprintf(stderr, "%s(): bad mode where net_id = %d\n", F, pl->item[i].net_id);
            success= 0;
        }
        if (pl->item[i].pud != DIO_PUD_UP) {
            fprintf(stderr, "%s(): bad PUD where net_id = %d (up expected)\n", F, pl->item[i].net_id);
            success= 0;
        }
        if (pl->item[i].pwm.period.tv_sec < 0 || pl->item[i].pwm.period.tv_nsec < 0) {
            fprintf(stderr, "%s(): bad pwm_period where net_id = %d\n", F, pl->item[i].net_id);
            success= 0;
        }
        if (pl->item[i].pwm.resolution < 0) {
            fprintf(stderr, "%s(): bad resolution where net_id = %d\n", F, pl->item[i].net_id);
            success= 0;
        }
    }

    FFORLISTPL ( pl,i, j ) {
            if (pl->item[i].net_id == pl->item[j].net_id) {
                fprintf(stderr, "%s(): net_id is not unique where net_id = %d\n", F, pl->item[i].net_id);
                success= 0;
            }
        }
    }

    FFORLISTPL ( pl,i, j ) {
            if (pl->item[i].id_dev == pl->item[j].id_dev && pl->item[i].device->id == pl->item[j].device->id) {
                fprintf(stderr, "%s(): id_within_device is not unique where net_id = %d\n", F, pl->item[i].net_id);
                success= 0;
            }
        }
    }
    return success;
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
        fprintf(stderr, "%s(): initDevPin\n", F);
#endif
        return 0;
    }
    FORLISTP(dl, i) {
        dl->item[i].i2c_fd = I2COpen(dl->item[i].i2c_path, dl->item[i].i2c_addr);
        if (dl->item[i].i2c_fd == -1) {
#ifdef MODE_DEBUG
            fprintf(stderr, "%s(): I2COpen\n", F);
#endif
            return 0;
        }
        dl->item[i].old_data1 = I2CRead(dl->item[i].i2c_fd);
        if (dl->item[i].old_data1 == -1) {
#ifdef MODE_DEBUG
            fprintf(stderr, "%s(): I2CRead\n", F);
#endif
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


