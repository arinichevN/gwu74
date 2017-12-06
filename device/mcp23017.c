
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
    old = I2CReadReg8(pin->device->fd_i2c, reg);
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
    I2CWriteReg8(pin->device->fd_i2c, reg, old);
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
    old = I2CReadReg8(pin->device->fd_i2c, reg);
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
    I2CWriteReg8(pin->device->fd_i2c, reg, old);
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
            I2CWriteReg8(list->item[i].fd_i2c, MCP23x17_GPIOA, list->item[i].new_data1);
            list->item[i].old_data1 = list->item[i].new_data1;
        }
        if (list->item[i].new_data2 != list->item[i].old_data2) {
            I2CWriteReg8(list->item[i].fd_i2c, MCP23x17_GPIOB, list->item[i].new_data2);
            list->item[i].old_data2 = list->item[i].new_data2;
        }
        if (list->item[i].new_data3 != list->item[i].old_data3) {
            I2CWriteReg8(list->item[i].fd_i2c, MCP23x17_IODIRA, list->item[i].new_data3);
            list->item[i].old_data3 = list->item[i].new_data3;
        }
        if (list->item[i].new_data4 != list->item[i].old_data4) {
            I2CWriteReg8(list->item[i].fd_i2c, MCP23x17_IODIRB, list->item[i].new_data4);
            list->item[i].old_data4 = list->item[i].new_data4;
        }
        if (list->item[i].new_data5 != list->item[i].old_data5) {
            I2CWriteReg8(list->item[i].fd_i2c, MCP23x17_GPPUA, list->item[i].new_data5);
            list->item[i].old_data5 = list->item[i].new_data5;
        }
        if (list->item[i].new_data6 != list->item[i].old_data6) {
            I2CWriteReg8(list->item[i].fd_i2c, MCP23x17_GPPUB, list->item[i].new_data6);
            list->item[i].old_data6 = list->item[i].new_data6;
        }
    }
}

void mcp23017_readDeviceList(DeviceList *list, PinList *pl) {
    int i, j;
    for (i = 0; i < list->length; i++) {
        if (list->item[i].read1) {
            int value = I2CReadReg8(list->item[i].fd_i2c, MCP23x17_GPIOA);
            for (j = 0; j < pl->length; j++) {
                if (pl->item[j].device->id == list->item[i].id && pl->item[j].mode == DIO_MODE_IN && pl->item[j].id_dev < 8) {
                    int id = pl->item[j].id_dev;
                    int mask = 1 << id;
                    if ((value & mask) == 0) {
                        pl->item[j].value = DIO_LOW;
                    } else {
                        pl->item[j].value = DIO_HIGH;
                    }
                    pl->item[j].tm=getCurrentTime();
                     pl->item[j].value_state=1;
                }
            }
            list->item[i].read1 = 0;
        }
        if (list->item[i].read2) {
            int value = I2CReadReg8(list->item[i].fd_i2c, MCP23x17_GPIOB);
            for (j = 0; j < pl->length; j++) {
                if (pl->item[j].device->id == list->item[i].id && pl->item[j].mode == DIO_MODE_IN && pl->item[j].id_dev >= 8) {
                    int id = pl->item[j].id_dev & 0x07;
                    int mask = 1 << id;
                    if ((value & mask) == 0) {
                        pl->item[j].value = DIO_LOW;
                    } else {
                        pl->item[j].value = DIO_HIGH;
                    }
                    pl->item[j].tm=getCurrentTime();
                    pl->item[j].value_state=1;
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
            fprintf(stderr, "ERROR: checkData: bad id_within_device where net_id = %d\n", pl->item[i].net_id);
            return 0;
        }
        if (pl->item[i].device == NULL) {
            fprintf(stderr, "ERROR: checkData: no device assigned to pin where net_id = %d\n", pl->item[i].net_id);
            return 0;
        }
        if (pl->item[i].mode != DIO_MODE_IN && pl->item[i].mode != DIO_MODE_OUT) {
            fprintf(stderr, "ERROR: checkData: bad mode where net_id = %d (in or out expected)\n", pl->item[i].net_id);
            return 0;
        }
        if (pl->item[i].pud != DIO_PUD_OFF && pl->item[i].pud != DIO_PUD_UP) {
            fprintf(stderr, "ERROR: checkData: bad PUD where net_id = %d (up or off expected)\n", pl->item[i].net_id);
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

void mcp23017_setPtf() {
    setMode = mcp23017_setMode;
    setPUD = mcp23017_setPUD;
    setOut = mcp23017_setOut;
    getIn = mcp23017_getIn;
    writeDeviceList = mcp23017_writeDeviceList;
    readDeviceList = mcp23017_readDeviceList;
}

/*
int mcp23017_initDevPin(DeviceList *dl, PinList *pl, PGconn *db_conn, char *app_class, char *i2c_path) {
    PGresult *r;
    char q[LINE_SIZE];
    size_t i;
    snprintf(q, sizeof q, "select id, addr_i2c from " APP_NAME_STR ".device where app_class='%s' limit %d", app_class, MCP23017_MAX_DEV_NUM);
    if ((r = dbGetDataT(db_conn, q, q)) == NULL) {
        return 0;
    }
    puts("params:");
    puts(i2c_path);
    puts(app_class);
    dl->length = PQntuples(r);
    printf("device list length: %d\n", dl->length);
    if (dl->length > 0) {
        dl->item = (Device *) malloc(dl->length * sizeof *(dl->item));
        if (dl->item == NULL) {
            fputs("ERROR: initDevPin: failed to allocate memory for devices\n", stderr);
            PQclear(r);
            return 0;
        }
        for (i = 0; i < dl->length; i++) {
            memset(&dl->item[i], 0, sizeof dl->item[i]);
            dl->item[i].id = atoi(PQgetvalue(r, i, 0));
            dl->item[i].fd_i2c = I2COpen(i2c_path, atoi(PQgetvalue(r, i, 1)));
            if (dl->item[i].fd_i2c == -1) {
                fputs("ERROR: initDevPin: I2COpen\n", stderr);
                PQclear(r);
                return 0;
            }
            I2CWriteReg8(dl->item[i].fd_i2c, MCP23x17_IOCON, IOCON_INIT);

            dl->item[i].old_data1 = I2CReadReg8(dl->item[i].fd_i2c, MCP23x17_OLATA);
            if (dl->item[i].old_data1 == -1) {
                fputs("ERROR: initDevPin: I2CReadReg8 1\n", stderr);
                PQclear(r);
                return 0;
            }
            dl->item[i].new_data1 = dl->item[i].old_data1;

            dl->item[i].old_data2 = I2CReadReg8(dl->item[i].fd_i2c, MCP23x17_OLATB);
            if (dl->item[i].old_data2 == -1) {
                fputs("ERROR: initDevPin: I2CReadReg8 2\n", stderr);
                PQclear(r);
                return 0;
            }
            dl->item[i].new_data2 = dl->item[i].old_data2;

            //mode
            dl->item[i].old_data3 = I2CReadReg8(dl->item[i].fd_i2c, MCP23x17_IODIRA);
            if (dl->item[i].old_data3 == -1) {
                fputs("ERROR: initDevPin: I2CReadReg8 3\n", stderr);
                PQclear(r);
                return 0;
            }
            dl->item[i].new_data3 = dl->item[i].old_data3;

            dl->item[i].old_data4 = I2CReadReg8(dl->item[i].fd_i2c, MCP23x17_IODIRB);
            if (dl->item[i].old_data4 == -1) {
                fputs("ERROR: initDevPin: I2CReadReg8 4\n", stderr);
                PQclear(r);
                return 0;
            }
            dl->item[i].new_data4 = dl->item[i].old_data4;

            //pud
            dl->item[i].old_data5 = I2CReadReg8(dl->item[i].fd_i2c, MCP23x17_GPPUA);
            if (dl->item[i].old_data5 == -1) {
                fputs("ERROR: initDevPin: I2CReadReg8 5\n", stderr);
                PQclear(r);
                return 0;
            }
            dl->item[i].new_data5 = dl->item[i].old_data5;

            dl->item[i].old_data6 = I2CReadReg8(dl->item[i].fd_i2c, MCP23x17_GPPUB);
            if (dl->item[i].old_data6 == -1) {
                fputs("ERROR: initDevPin: I2CReadReg8 6\n", stderr);
                PQclear(r);
                return 0;
            }
            dl->item[i].new_data6 = dl->item[i].old_data6;

        }
    }
    PQclear(r);
    snprintf(q, sizeof q, "select net_id, device_id, id_within_device, mode, pud, pwm_period_sec, pwm_period_nsec from " APP_NAME_STR ".pin where app_class='%s' limit %d", app_class, MCP23017_MAX_PIN_NUM);
    if ((r = dbGetDataT(db_conn, q, q)) == NULL) {
        return 0;
    }
    pl->length = PQntuples(r);
    printf("pin list length: %d\n", pl->length);
    if (pl->length > 0) {
        pl->item = (Pin *) malloc(pl->length * sizeof *(pl->item));
        if (pl->item == NULL) {
            fputs("ERROR: initDevPin: failed to allocate memory for pins\n", stderr);
            PQclear(r);
            return 0;
        }
        for (i = 0; i < pl->length; i++) {
            memset(&pl->item[i], 0, sizeof pl->item[i]);
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
    if (!mcp23017_checkData(dl, pl)) {
        return 0;
    }
    mcp23017_setPtf();
    return 1;
}
 */

int mcp23017_initDevPin(DeviceList *dl, PinList *pl, const char *db_path, char *i2c_path) {
    sqlite3 *db;
    if (!db_open(db_path, &db)) {
        return 0;
    }
    size_t i;
    int n = 0;
    char q[LINE_SIZE];
    db_getInt(&n, db, "select count(*) from device");
    db_getInt(&n, db, q);
    if (n <= 0) {
        putse("mcp23017_initDevPin: query failed: select count(*) from device\n");
        sqlite3_close(db);
        return 0;
    }
    dl->length = 0;
    dl->item = (Device *) malloc(n * sizeof *(dl->item));
    if (dl->item == NULL) {
        putse("ERROR: mcp23017_initDevPin: failed to allocate memory for devices\n");
        sqlite3_close(db);
        return 0;
    }
    memset(dl->item, 0, n * sizeof *(dl->item));
    DeviceData data = {dl, i2c_path};
    snprintf(q, sizeof q, "select id, addr_i2c from device limit %d", MCP23017_MAX_DEV_NUM);
    if (!db_exec(db, q, getDevice_callback, (void*) &data)) {
        printfe("mcp23017_initDevPin: query failed: %s\n", q);
        sqlite3_close(db);
        return 0;
    }
    if (dl->length != n) {
        printfe("mcp23017_initDevPin: %ld != %ld\n", dl->length, n);
        sqlite3_close(db);
        return 0;
    }
    for (i = 0; i < dl->length; i++) {
        if (dl->item[i].fd_i2c == -1) {
            putse("mcp23017_initDevPin: I2COpen failed\n");
            sqlite3_close(db);
            return 0;
        }
        I2CWriteReg8(dl->item[i].fd_i2c, MCP23x17_IOCON, IOCON_INIT);

        dl->item[i].old_data1 = I2CReadReg8(dl->item[i].fd_i2c, MCP23x17_OLATA);
        if (dl->item[i].old_data1 == -1) {
            putse("ERROR: initDevPin: I2CReadReg8 1\n");
            sqlite3_close(db);
            return 0;
        }
        dl->item[i].new_data1 = dl->item[i].old_data1;

        dl->item[i].old_data2 = I2CReadReg8(dl->item[i].fd_i2c, MCP23x17_OLATB);
        if (dl->item[i].old_data2 == -1) {
            putse("ERROR: initDevPin: I2CReadReg8 2\n");
            sqlite3_close(db);
            return 0;
        }
        dl->item[i].new_data2 = dl->item[i].old_data2;

        //mode
        dl->item[i].old_data3 = I2CReadReg8(dl->item[i].fd_i2c, MCP23x17_IODIRA);
        if (dl->item[i].old_data3 == -1) {
            putse("ERROR: initDevPin: I2CReadReg8 3\n");
            sqlite3_close(db);
            return 0;
        }
        dl->item[i].new_data3 = dl->item[i].old_data3;

        dl->item[i].old_data4 = I2CReadReg8(dl->item[i].fd_i2c, MCP23x17_IODIRB);
        if (dl->item[i].old_data4 == -1) {
            putse("ERROR: initDevPin: I2CReadReg8 4\n");
            sqlite3_close(db);
            return 0;
        }
        dl->item[i].new_data4 = dl->item[i].old_data4;

        //pud
        dl->item[i].old_data5 = I2CReadReg8(dl->item[i].fd_i2c, MCP23x17_GPPUA);
        if (dl->item[i].old_data5 == -1) {
            putse("ERROR: initDevPin: I2CReadReg8 5\n");
            sqlite3_close(db);
            return 0;
        }
        dl->item[i].new_data5 = dl->item[i].old_data5;

        dl->item[i].old_data6 = I2CReadReg8(dl->item[i].fd_i2c, MCP23x17_GPPUB);
        if (dl->item[i].old_data6 == -1) {
            putse("ERROR: initDevPin: I2CReadReg8 6\n");
            sqlite3_close(db);
            return 0;
        }
        dl->item[i].new_data6 = dl->item[i].old_data6;

    }

    n = 0;
    db_getInt(&n, db, "select count(*) from pin");
    if (n <= 0) {
        putse("mcp23017_initDevPin: query failed: select count(*) from pin\n");
        sqlite3_close(db);
        return 0;
    }
    pl->item = (Pin *) malloc(n * sizeof *(pl->item));
    if (pl->item == NULL) {
        putse("mcp23017_initDevPin: failed to allocate memory for pins\n");
        sqlite3_close(db);
        return 0;
    }
    memset(pl->item, 0, n * sizeof *(pl->item));
    pl->length = 0;
    PinData datap = {pl, dl};
    snprintf(q, sizeof q, PIN_QUERY_STR, MCP23017_MAX_PIN_NUM);
    if (!db_exec(db, q, getPin_callback, (void*) &datap)) {
        printfe("mcp23017_initDevPin: query failed: %s\n", q);
        sqlite3_close(db);
        return 0;
    }
    if (pl->length != n) {
        printfe("mcp23017_initDevPin: %ld != %ld\n", pl->length, n);
        sqlite3_close(db);
        return 0;
    }
    sqlite3_close(db);
    if (!mcp23017_checkData(dl, pl)) {
        return 0;
    }
    mcp23017_setPtf();
    return 1;
}
