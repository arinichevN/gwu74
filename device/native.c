
void native_setMode(Pin *pin, int mode) {
    switch (mode) {
        case DIO_MODE_IN:
            pinModeIn(pin->id_dev);
            break;
        case DIO_MODE_OUT:
            pinModeOut(pin->id_dev);
            break;
        default:
#ifdef MODE_DEBUG
            fputs("setMode: bad mode\n", stderr);
#endif
            break;
    }
}

void native_setPUD(Pin *pin, int pud) {
    switch (pud) {
        case DIO_PUD_OFF:
            pinPUD(pin->id_dev, PUD_OFF);
            break;
        case DIO_PUD_UP:
            pinPUD(pin->id_dev, PUD_UP);
            break;
        case DIO_PUD_DOWN:
            pinPUD(pin->id_dev, PUD_DOWN);
            break;
        default:
#ifdef MODE_DEBUG
            fputs("setPUD: bad PUD\n", stderr);
#endif
            break;
    }
    return;
}

void native_setOut(Pin *pin, int value) {
    switch (value) {
        case DIO_HIGH:
            pinHigh(pin->id_dev);
            break;
        case DIO_LOW:
            pinLow(pin->id_dev);
            break;
        default:
#ifdef MODE_DEBUG
            fputs("setOut: bad value\n", stderr);
#endif
            break;
    }
}

void native_getIn(Pin *pin) {
    pin->value = pinRead(pin->id_dev);
}

void native_writeDeviceList(DeviceList *list) {
    ;
}

void native_readDeviceList(DeviceList *list, PinList *pl) {
    ;
}

int native_checkData(PinList *list) {
    size_t i, j;
    for (i = 0; i < list->length; i++) {
        if (!checkPin(list->item[i].id_dev)) {
            fprintf(stderr, "ERROR: checkData: bad id_within_device where net_id = %d\n", list->item[i].net_id);
            return 0;
        }
        if (list->item[i].mode != DIO_MODE_IN && list->item[i].mode != DIO_MODE_OUT) {
            fprintf(stderr, "ERROR: checkData: bad mode where net_id = %d\n", list->item[i].net_id);
            return 0;
        }
        if (list->item[i].pud != DIO_PUD_OFF && list->item[i].pud != DIO_PUD_UP && list->item[i].pud != DIO_PUD_DOWN) {
            fprintf(stderr, "ERROR: checkData: bad PUD where net_id = %d\n", list->item[i].net_id);
            return 0;
        }
        if (list->item[i].pwm.period.tv_sec < 0 || list->item[i].pwm.period.tv_nsec < 0) {
            fprintf(stderr, "ERROR: checkData: bad pwm_period where net_id = %d\n", list->item[i].net_id);
            return 0;
        }
        if (list->item[i].pwm.rsl < 0) {
            fprintf(stderr, "ERROR: checkData: bad rsl where net_id = %d\n", list->item[i].net_id);
            return 0;
        }
    }
    for (i = 0; i < list->length; i++) {
        for (j = i + 1; j < list->length; j++) {
            if (list->item[i].net_id == list->item[j].net_id) {
                fprintf(stderr, "ERROR: checkData: net_id is not unique where net_id = %d\n", list->item[i].net_id);
                return 0;
            }
        }
    }
    for (i = 0; i < list->length; i++) {
        for (j = i + 1; j < list->length; j++) {
            if (list->item[i].id_dev == list->item[j].id_dev) {
                fprintf(stderr, "ERROR: checkPin: id_within_device is not unique where net_id = %d\n", list->item[i].net_id);
                return 0;
            }
        }
    }
    return 1;
}

void native_setPtf() {
    setMode = native_setMode;
    setPUD = native_setPUD;
    setOut = native_setOut;
    getIn = native_getIn;
    writeDeviceList = native_writeDeviceList;
    readDeviceList = native_readDeviceList;
}
/*
 * see mcp23017.c to make this function to be able to work with sqlite 
 */

/*
int native_initDevPin(DeviceList *dl, PinList *pl, PGconn *db_conn, char *app_class) {
    PGresult *r;
    char q[LINE_SIZE];
    size_t i;
    snprintf(q, sizeof q, "select net_id, id_within_device, mode, pud, rsl, pwm_period_sec, pwm_period_nsec from " APP_NAME_STR ".pin where app_class='%s' limit %d", app_class, NATIVE_MAX_PIN_NUM);
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
            pl->item[i].id_dev = atoi(PQgetvalue(r, i, 1));
            pl->item[i].mode = getModeByStr(PQgetvalue(r, i, 2));
            pl->item[i].pud = getPUDByStr(PQgetvalue(r, i, 3));
            pl->item[i].pwm.period.tv_sec = atoi(PQgetvalue(r, i, 4));
            pl->item[i].pwm.period.tv_nsec = atoi(PQgetvalue(r, i, 5));
            pl->item[i].device = NULL;
        }
    }
    PQclear(r);
    if (!native_checkData(pl)) {
        return 0;
    }
    if (!gpioSetup()) {
        return 0;
    }
    native_setPtf();
    return 1;
}
 */

int native_initDevPin(DeviceList *dl, PinList *pl, const char *db_path) {
    putse("not emplemented yet");
    return 0;
}