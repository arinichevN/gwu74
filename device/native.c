
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
    pin->tm = getCurrentTime();
    pin->value_state = 1;
}

void native_writeDeviceList(DeviceList *list) {
    ;
}

void native_readDeviceList(DeviceList *list, PinList *pl) {
    ;
}

int native_checkData(PinList *list) {
    for (size_t i = 0; i < list->length; i++) {
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
    for (size_t i = 0; i < list->length; i++) {
        for (size_t j = i + 1; j < list->length; j++) {
            if (list->item[i].net_id == list->item[j].net_id) {
                fprintf(stderr, "ERROR: checkData: net_id is not unique where net_id = %d\n", list->item[i].net_id);
                return 0;
            }
        }
    }
    for (size_t i = 0; i < list->length; i++) {
        for (size_t j = i + 1; j < list->length; j++) {
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
        for (size_t i = 0; i < pl->length; i++) {
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
    sqlite3 *db;
    if (!db_open(db_path, &db)) {
        return 0;
    }
    int n = 0;
    char q[LINE_SIZE];
        dl->length = 0;
    dl->item = NULL;
    db_getInt(&n, db, "select count(*) from pin");
    if (n <= 0) {
        putse("native_initDevPin: query failed: select count(*) from pin\n");
        sqlite3_close(db);
        return 0;
    }
    pl->item = (Pin *) malloc(n * sizeof *(pl->item));
    if (pl->item == NULL) {
        putse("native_initDevPin: failed to allocate memory for pins\n");
        sqlite3_close(db);
        return 0;
    }
    memset(pl->item, 0, n * sizeof *(pl->item));
    pl->length = 0;
    PinData datap = {pl, dl};
    snprintf(q, sizeof q, PIN_QUERY_STR, NATIVE_MAX_PIN_NUM);
    if (!db_exec(db, q, getPin_callback, (void*) &datap)) {
        printfe("native_initDevPin: query failed: %s\n", q);
        sqlite3_close(db);
        return 0;
    }
    if (pl->length != n) {
        printfe("native_initDevPin: %ld != %d\n", dl->length, n);
        sqlite3_close(db);
        return 0;
    }
    for (size_t i = 0; i < pl->length; i++) {
        pl->item[i].device = NULL;
    }
    sqlite3_close(db);

    if (!native_checkData(pl)) {
        return 0;
    }
    if (!gpioSetup()) {
        return 0;
    }
    native_setPtf();
    return 1;
}
