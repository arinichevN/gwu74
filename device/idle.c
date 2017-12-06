
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
    for (size_t i = 0; i < pl->length; i++) {
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
        if (pl->item[i].pwm.rsl < 0) {
            fprintf(stderr, "ERROR: checkData: bad rsl where net_id = %d\n", pl->item[i].net_id);
            return 0;
        }
    }
    for (size_t i = 0; i < pl->length; i++) {
        for (size_t j = i + 1; j < pl->length; j++) {
            if (pl->item[i].net_id == pl->item[j].net_id) {
                fprintf(stderr, "ERROR: checkDevPin: net_id is not unique where net_id = %d\n", pl->item[i].net_id);
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
    sqlite3 *db;
    if (!db_open(db_path, &db)) {
        return 0;
    }
    int n = 0;
    char q[LINE_SIZE];
    dl->length = 0;
    dl->item = NULL;

    n = 0;
    db_getInt(&n, db, "select count(*) from pin");
    if (n <= 0) {
        putse("idle_initDevPin: query failed: select count(*) from pin\n");
        sqlite3_close(db);
        return 0;
    }
    pl->item = (Pin *) malloc(n * sizeof *(pl->item));
    if (pl->item == NULL) {
        putse("idle_initDevPin: failed to allocate memory for pins\n");
        sqlite3_close(db);
        return 0;
    }
    memset(pl->item, 0, n * sizeof *(pl->item));
    pl->length = 0;
    PinData datap = {pl, dl};
    snprintf(q, sizeof q, PIN_QUERY_STR, IDLE_MAX_PIN_NUM);
    if (!db_exec(db, q, getPin_callback, (void*) &datap)) {
        printfe("idle_initDevPin: query failed: %s\n", q);
        sqlite3_close(db);
        return 0;
    }
    if (pl->length != n) {
        printfe("idle_initDevPin: %ld != %d\n", dl->length, n);
        sqlite3_close(db);
        return 0;
    }
        for (size_t i = 0; i < pl->length; i++) {
            pl->item[i].device=NULL;
        }
    sqlite3_close(db);
    if (!idle_checkDevPin(dl, pl)) {
        return 0;
    }
    idle_setPtf();
    return 1;
}


/*
int idle_initDevPin(DeviceList *dl, PinList *pl, PGconn *db_conn, char *app_class) {
    PGresult *r;
    char q[LINE_SIZE];
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
        for (size_t i = 0; i < dl->length; i++) {
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
        for (size_t i = 0; i < pl->length; i++) {
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
 */

