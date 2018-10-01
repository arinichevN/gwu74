
#include "main.h"

void mcp23017_setMode(Pin *pin, int mode) {
    int mask, old, reg, id;
    id = pin->id_dev;
    if (id < 8) {
        reg = MCP23x17_IODIRA;
    } else {
        reg = MCP23x17_IODIRB;
        id &= 0x07;
    }

    mask = 1 << id;
    old = I2CReadReg8(pin->device->i2c_fd, reg);
    switch (mode) {
        case DIO_MODE_OUT:
            old &= (~mask);
            break;
        case DIO_MODE_IN:
            old |= mask;
            break;
        default:
            return;
    }
    I2CWriteReg8(pin->device->i2c_fd, reg, old);
}

void mcp23017_setPUD(Pin *pin, int pud) {
    int mask, old, reg, id;
    id = pin->id_dev;
    if (id < 8) {
        reg = MCP23x17_GPPUA;
    } else {
        reg = MCP23x17_GPPUB;
        id &= 0x07;
    }
    mask = 1 << id;
    old = I2CReadReg8(pin->device->i2c_fd, reg);
    switch (pud) {
        case DIO_PUD_UP:
            old |= mask;
            break;
        case DIO_PUD_OFF:
            old &= (~mask);
            break;
        default:
            return;
    }
    I2CWriteReg8(pin->device->i2c_fd, reg, old);
}

/*
 * call writeDeviceList() function after this function and then data will be written to chip
 */
void mcp23017_setOut(Pin *pin, int value) {
    int bit, id;
    id = pin->id_dev;
    bit = 1 << (id & 7);
    if (id < 8) {
        if (value == DIO_LOW) {
            pin->device->new_data1 &= (~bit);
        } else {
            pin->device->new_data1 |= bit;
        }
    } else {
        if (value == DIO_LOW) {
            pin->device->new_data2 &= (~bit);
        } else {
            pin->device->new_data2 |= bit;
        }
    }
}

/*
 * call readDeviceList() after this function and then pin->value will be updated
 */
void mcp23017_getIn(Pin *pin) {
    if (pin->id_dev < 8) {
        pin->device->read1 = 1;
    } else {
        pin->device->read2 = 1;
    }

}

void mcp23017_writeDeviceList(DeviceList *list) {
    int i;
    for (i = 0; i < list->length; i++) {
        if (list->item[i].new_data1 != list->item[i].old_data1) {
            I2CWriteReg8(list->item[i].i2c_fd, MCP23x17_GPIOA, list->item[i].new_data1);
            list->item[i].old_data1 = list->item[i].new_data1;
        }
        if (list->item[i].new_data2 != list->item[i].old_data2) {
            I2CWriteReg8(list->item[i].i2c_fd, MCP23x17_GPIOB, list->item[i].new_data2);
            list->item[i].old_data2 = list->item[i].new_data2;
        }
        if (list->item[i].new_data3 != list->item[i].old_data3) {
            I2CWriteReg8(list->item[i].i2c_fd, MCP23x17_IODIRA, list->item[i].new_data3);
            list->item[i].old_data3 = list->item[i].new_data3;
        }
        if (list->item[i].new_data4 != list->item[i].old_data4) {
            I2CWriteReg8(list->item[i].i2c_fd, MCP23x17_IODIRB, list->item[i].new_data4);
            list->item[i].old_data4 = list->item[i].new_data4;
        }
        if (list->item[i].new_data5 != list->item[i].old_data5) {
            I2CWriteReg8(list->item[i].i2c_fd, MCP23x17_GPPUA, list->item[i].new_data5);
            list->item[i].old_data5 = list->item[i].new_data5;
        }
        if (list->item[i].new_data6 != list->item[i].old_data6) {
            I2CWriteReg8(list->item[i].i2c_fd, MCP23x17_GPPUB, list->item[i].new_data6);
            list->item[i].old_data6 = list->item[i].new_data6;
        }
    }
}

void mcp23017_readDeviceList(DeviceList *list, PinList *pl) {
    int i, j;
    for (i = 0; i < list->length; i++) {
        if (list->item[i].read1) {
            int value = I2CReadReg8(list->item[i].i2c_fd, MCP23x17_GPIOA);
            for (j = 0; j < pl->length; j++) {
                if (pl->item[j].device->id == list->item[i].id && pl->item[j].mode == DIO_MODE_IN && pl->item[j].id_dev < 8) {
                    int id = pl->item[j].id_dev;
                    int mask = 1 << id;
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
        if (list->item[i].read2) {
            int value = I2CReadReg8(list->item[i].i2c_fd, MCP23x17_GPIOB);
            for (j = 0; j < pl->length; j++) {
                if (pl->item[j].device->id == list->item[i].id && pl->item[j].mode == DIO_MODE_IN && pl->item[j].id_dev >= 8) {
                    int id = pl->item[j].id_dev & 0x07;
                    int mask = 1 << id;
                    if ((value & mask) == 0) {
                        pl->item[j].value = DIO_LOW;
                    } else {
                        pl->item[j].value = DIO_HIGH;
                    }
                    pl->item[j].tm = getCurrentTime();
                    pl->item[j].value_state = 1;
                }
            }
            list->item[i].read2 = 0;
        }
    }
}

int mcp23017_checkData(DeviceList *dl, PinList *pl) {
    size_t i, j;
    for (i = 0; i < pl->length; i++) {
        if (pl->item[i].id_dev < 0 || pl->item[i].id_dev >= MCP23017_DEVICE_PIN_NUM) {
            fprintf(stderr, "%s(): bad id_within_device where net_id = %d\n",F, pl->item[i].net_id);
            return 0;
        }
        if (pl->item[i].device == NULL) {
            fprintf(stderr, "%s(): no device assigned to pin where net_id = %d\n",F, pl->item[i].net_id);
            return 0;
        }
        if (pl->item[i].mode != DIO_MODE_IN && pl->item[i].mode != DIO_MODE_OUT) {
            fprintf(stderr, "%s(): bad mode where net_id = %d (in or out expected)\n",F, pl->item[i].net_id);
            return 0;
        }
        if (pl->item[i].pud != DIO_PUD_OFF && pl->item[i].pud != DIO_PUD_UP) {
            fprintf(stderr, "%s(): bad PUD where net_id = %d (up or off expected)\n",F, pl->item[i].net_id);
            return 0;
        }
        if (pl->item[i].pwm.period.tv_sec < 0 || pl->item[i].pwm.period.tv_nsec < 0) {
            fprintf(stderr, "%s(): bad pwm_period where net_id = %d\n",F, pl->item[i].net_id);
            return 0;
        }
        if (pl->item[i].pwm.resolution < 0) {
            fprintf(stderr, "%s(): bad resolution where net_id = %d\n",F, pl->item[i].net_id);
            return 0;
        }
    }
    for (i = 0; i < pl->length; i++) {
        for (j = i + 1; j < pl->length; j++) {
            if (pl->item[i].net_id == pl->item[j].net_id) {
                fprintf(stderr, "%s(): net_id is not unique where net_id = %d\n",F, pl->item[i].net_id);
                return 0;
            }
        }
    }
    for (i = 0; i < pl->length; i++) {
        for (j = i + 1; j < pl->length; j++) {
            if (pl->item[i].id_dev == pl->item[j].id_dev && pl->item[i].device->id == pl->item[j].device->id) {
                fprintf(stderr, "%s(): id_within_device is not unique where net_id = %d\n",F, pl->item[i].net_id);
                return 0;
            }
        }
    }
    return 1;
}

void mcp23017_setPtf() {
    setMode = mcp23017_setMode;
    setPUD = mcp23017_setPUD;
    setOut = mcp23017_setOut;
    getIn = mcp23017_getIn;
    writeDeviceList = mcp23017_writeDeviceList;
    readDeviceList = mcp23017_readDeviceList;
}

int mcp23017_initDevPin(DeviceList *dl, PinList *pl, const char *db_path) {
    if (!initDevPin(dl, pl, IDLE_MAX_DEV_NUM, IDLE_MAX_PIN_NUM, db_path)) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): initDevPin\n", F);
#endif
        return 0;
    }
    for (int i = 0; i < dl->length; i++) {
        dl->item[i].i2c_fd = I2COpen(dl->item[i].i2c_path, dl->item[i].i2c_addr);
        if (dl->item[i].i2c_fd == -1) {
#ifdef MODE_DEBUG
            fprintf(stderr, "%s(): I2COpen\n", F);
#endif
            return 0;
        }
        I2CWriteReg8(dl->item[i].i2c_fd, MCP23x17_IOCON, IOCON_INIT);

        dl->item[i].old_data1 = I2CReadReg8(dl->item[i].i2c_fd, MCP23x17_OLATA);
        if (dl->item[i].old_data1 == -1) {
#ifdef MODE_DEBUG
            fprintf(stderr, "%s(): I2CReadReg8 1\n", F);
#endif
            return 0;
        }
        dl->item[i].new_data1 = dl->item[i].old_data1;

        dl->item[i].old_data2 = I2CReadReg8(dl->item[i].i2c_fd, MCP23x17_OLATB);
        if (dl->item[i].old_data2 == -1) {
#ifdef MODE_DEBUG
            fprintf(stderr, "%s(): I2CReadReg8 2\n", F);
#endif
            return 0;
        }
        dl->item[i].new_data2 = dl->item[i].old_data2;

        //mode
        dl->item[i].old_data3 = I2CReadReg8(dl->item[i].i2c_fd, MCP23x17_IODIRA);
        if (dl->item[i].old_data3 == -1) {
#ifdef MODE_DEBUG
            fprintf(stderr, "%s(): I2CReadReg8 3\n", F);
#endif
            return 0;
        }
        dl->item[i].new_data3 = dl->item[i].old_data3;

        dl->item[i].old_data4 = I2CReadReg8(dl->item[i].i2c_fd, MCP23x17_IODIRB);
        if (dl->item[i].old_data4 == -1) {
#ifdef MODE_DEBUG
            fprintf(stderr, "%s(): I2CReadReg8 4\n", F);
#endif
            return 0;
        }
        dl->item[i].new_data4 = dl->item[i].old_data4;

        //pud
        dl->item[i].old_data5 = I2CReadReg8(dl->item[i].i2c_fd, MCP23x17_GPPUA);
        if (dl->item[i].old_data5 == -1) {
#ifdef MODE_DEBUG
            fprintf(stderr, "%s(): I2CReadReg8 5\n", F);
#endif
            return 0;
        }
        dl->item[i].new_data5 = dl->item[i].old_data5;

        dl->item[i].old_data6 = I2CReadReg8(dl->item[i].i2c_fd, MCP23x17_GPPUB);
        if (dl->item[i].old_data6 == -1) {
#ifdef MODE_DEBUG
            fprintf(stderr, "%s(): I2CReadReg8 6\n", F);
#endif
            return 0;
        }
        dl->item[i].new_data6 = dl->item[i].old_data6;

    }
    if (!mcp23017_checkData(dl, pl)) {
        return 0;
    }
    mcp23017_setPtf();
    return 1;
}
