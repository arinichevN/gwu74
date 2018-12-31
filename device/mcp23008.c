
void mcp23008_setMode(Pin *pin, int mode) {
    int reg = MCP23x08_IODIR;
    int mask = 1 << (pin->id_dev);
    int old = I2CReadReg8(pin->device->i2c_fd, reg);
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

void mcp23008_setPUD(Pin *pin, int pud) {
    int reg = MCP23x08_GPPU;
    int mask = 1 << (pin->id_dev);
    int old = I2CReadReg8(pin->device->i2c_fd, reg);
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

//call writeDeviceList() function after this function and then data will be written to chip

void mcp23008_setOut(Pin *pin, int value) {
   int  bit = 1 << (pin->id_dev & 7);
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

void mcp23008_getIn(Pin *pin) {
    pin->device->read1 = 1;
}

void mcp23008_writeDeviceList(DeviceList *list) {
    FORLi {
        if (LIi.new_data1 != LIi.old_data1) {
            I2CWriteReg8(LIi.i2c_fd, MCP23x08_GPIO, LIi.new_data1);
            LIi.old_data1 = LIi.new_data1;
        }
    }
}

void mcp23008_readDeviceList(DeviceList *list, PinList *pl) {
    FORLi {
        if (list->item[i].read1) {
            int value = I2CReadReg8(list->item[i].i2c_fd, MCP23x08_GPIO);
            FORLISTP(pl, j) {
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

int mcp23008_checkData(DeviceList *dl, PinList *pl) {
    int success=1;
    FORLISTP(pl, i) {
        if (pl->item[i].id_dev < 0 || pl->item[i].id_dev >= MCP23008_DEVICE_PIN_NUM) {
            fprintf(stderr, "%s(): bad id_within_device where net_id = %d\n",F, pl->item[i].net_id);
            success= 0;
        }
        if (pl->item[i].device == NULL) {
            fprintf(stderr, "%s(): no device assigned to pin where net_id = %d\n",F, pl->item[i].net_id);
            success= 0;
        }
        if (pl->item[i].mode != DIO_MODE_IN && pl->item[i].mode != DIO_MODE_OUT) {
            fprintf(stderr, "%s(): bad mode where net_id = %d\n",F, pl->item[i].net_id);
            success= 0;
        }
        if (pl->item[i].pud != DIO_PUD_OFF && pl->item[i].pud != DIO_PUD_UP) {
            fprintf(stderr, "%s(): bad PUD where net_id = %d (up or off expected)\n",F, pl->item[i].net_id);
            success= 0;
        }
        if (pl->item[i].pwm.period.tv_sec < 0 || pl->item[i].pwm.period.tv_nsec < 0) {
            fprintf(stderr, "%s(): bad pwm_period where net_id = %d\n",F, pl->item[i].net_id);
            success= 0;
        }
        if (pl->item[i].pwm.resolution < 0) {
            fprintf(stderr, "%s(): bad resolution where net_id = %d\n", F,pl->item[i].net_id);
            success= 0;
        }
    }
    FORLISTP(pl, i) {
        FORLISTPL ( pl, i, j ) {
            if (pl->item[i].net_id == pl->item[j].net_id) {
                fprintf(stderr, "%s(): net_id is not unique where net_id = %d\n",F, pl->item[i].net_id);
                success= 0;
            }
        }
    }
    FFORLISTPL ( pl,i, j ) {
            if (pl->item[i].id_dev == pl->item[j].id_dev && pl->item[i].device->id == pl->item[j].device->id) {
                fprintf(stderr, "%s(): id_within_device is not unique where net_id = %d\n",F, pl->item[i].net_id);
                success= 0;
            }
        }
    }
    return success;
}

void mcp23008_setPtf() {
    setMode = mcp23008_setMode;
    setPUD = mcp23008_setPUD;
    setOut = mcp23008_setOut;
    getIn = mcp23008_getIn;
    writeDeviceList = mcp23008_writeDeviceList;
    readDeviceList = mcp23008_readDeviceList;
}

int mcp23008_initDevPin(DeviceList *dl, PinList *pl, const char *db_path) {
    if (!initDevPin(dl, pl, MCP23008_MAX_DEV_NUM, MCP23008_MAX_PIN_NUM, db_path)) {
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
        I2CWriteReg8(dl->item[i].i2c_fd, MCP23x08_IOCON, IOCON_INIT);
        dl->item[i].old_data1 = I2CReadReg8(dl->item[i].i2c_fd, MCP23x08_OLAT);
        dl->item[i].new_data1 = dl->item[i].old_data1;
        if (dl->item[i].old_data1 == -1) {
#ifdef MODE_DEBUG
            fprintf(stderr, "%s(): I2CReadReg8\n", F);
#endif
            return 0;
        }
    }

    if (!mcp23008_checkData(dl, pl)) {
        return 0;
    }
    mcp23008_setPtf();
    return 1;
}
