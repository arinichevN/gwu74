
 void idle_setMode(Pin *pin, int mode) {
    pin->device->new_data1++;
}

 void idle_setPUD(Pin *pin, int pud) {
    pin->device->new_data1++;
}

//call writeDeviceList() function after this function and then data will be written to chip

 void idle_setOut(Pin *pin, int value) {
    pin->device->new_data1++;
}

//call readDeviceList() after this function and then pin->value will be updated

 void idle_getIn(Pin *pin) {
    pin->device->read1 = 1;
}

 void idle_writeDeviceList(DeviceList *list) {
    int i;
    for (i = 0; i < list->length; i++) {
        if (list->item[i].new_data1 != list->item[i].old_data1) {
            list->item[i].old_data1 = list->item[i].new_data1;
        }
    }
}

 void idle_readDeviceList(DeviceList *list, PinList *pl) {
    int i, j;
    for (i = 0; i < list->length; i++) {
        if (list->item[i].read1) {
            for (j = 0; j < pl->length; j++) {
                if (pl->item[j].device->id == list->item[i].id) {
                    pl->item[j].value = DIO_HIGH;
                }
            }
            list->item[i].read1 = 0;
        }
    }
}

 int idle_checkDevPin(DeviceList *dl, PinList *pl) {
    size_t i, j;
    for (i = 0; i < pl->length; i++) {
        if (pl->item[i].device == NULL) {
            fprintf(stderr, "ERROR: checkDevPin: no device assigned to pin where net_id = %d\n", pl->item[i].net_id);
            return 0;
        }
        if (pl->item[i].mode != DIO_MODE_IN && pl->item[i].mode != DIO_MODE_OUT) {
            fprintf(stderr, "ERROR: checkDevPin: bad mode where net_id = %d\n", pl->item[i].net_id);
            return 0;
        }
        if (pl->item[i].pud != DIO_PUD_OFF && pl->item[i].pud != DIO_PUD_UP && pl->item[i].pud != DIO_PUD_DOWN) {
            fprintf(stderr, "ERROR: checkDevPin: bad PUD where net_id = %d\n", pl->item[i].net_id);
            return 0;
        }
        if (pl->item[i].pwm.period.tv_sec < 0 || pl->item[i].pwm.period.tv_nsec < 0) {
            fprintf(stderr, "ERROR: checkDevPin: bad pwm_period where net_id = %d\n", pl->item[i].net_id);
            return 0;
        }
    }
    for (i = 0; i < pl->length; i++) {
        for (j = i + 1; j < pl->length; j++) {
            if (pl->item[i].net_id == pl->item[j].net_id) {
                fprintf(stderr, "ERROR: checkDevPin: net_id is not unique where net_id = %d\n", pl->item[i].net_id);
                return 0;
            }
        }
    }
    for (i = 0; i < pl->length; i++) {
        for (j = i + 1; j < pl->length; j++) {
            if (pl->item[i].id_dev == pl->item[j].id_dev && pl->item[i].device->id == pl->item[j].device->id) {
                fprintf(stderr, "ERROR: checkPin: id_within_device is not unique where net_id = %d\n", pl->item[i].net_id);
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

int idle_initDevPin(DeviceList *dl, PinList *pl, PGconn *db_conn, char *app_class) {
    PGresult *r;
    char q[LINE_SIZE];
    size_t i;
    snprintf(q, sizeof q, "select id, addr_i2c from " APP_NAME_STR ".device where app_class='%s' limit %d", app_class, IDLE_MAX_DEV_NUM);
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
            dl->item[i].fd_i2c = atoi(PQgetvalue(r, i, 1));
            dl->item[i].old_data1 = 0;
            dl->item[i].new_data1 = dl->item[i].old_data1;
        }
    }
    PQclear(r);
    snprintf(q, sizeof q, "select net_id, device_id, id_within_device, mode, pud, pwm_period_sec, pwm_period_nsec from " APP_NAME_STR ".pin where app_class='%s' limit %d", app_class, IDLE_MAX_PIN_NUM);
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
    if (!idle_checkDevPin(dl, pl)) {
        return 0;
    }
    idle_setPtf();
    return 1;
}
