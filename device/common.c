
#include "main.h"
#define PIN_QUERY_STR "select net_id, device_id, id_within_device, mode, pud, rsl, pwm_period_sec, pwm_period_nsec, secure_timeout_sec, secure_duty_cycle, secure_enable from pin limit %d"

typedef struct {
    DeviceList *list;
} DeviceData;

typedef struct {
    PinList *list;
    DeviceList *device_list;
} PinData;

FUN_LIST_INIT(Device)
FUN_LIST_INIT(Pin)

int getDevice_callback(void *data, int argc, char **argv, char **azColName) {
    DeviceData *device_data = data;
    int c = 0;
    for (int i = 0; i < argc; i++) {
        if (DB_COLUMN_IS("id")) {
            device_data->list->item[device_data->list->length].id = atoi(argv[i]);
            c++;
        } else if (DB_COLUMN_IS("i2c_addr")) {
            device_data->list->item[device_data->list->length].i2c_addr = atoi(argv[i]);
            // device_data->list->item[device_data->list->length].i2c_fd = I2COpen(device_data->i2c_path, atoi(argv[i]));
            c++;
        } else if (DB_COLUMN_IS("i2c_path")) {
            memcpy(device_data->list->item[device_data->list->length].i2c_path, argv[i], sizeof device_data->list->item[device_data->list->length].i2c_path);
            c++;
        } else {
#ifdef MODE_DEBUG
            fprintf(stderr, "%s(): unknown column\n", __FUNCTION__);
#endif
        }
    }
    device_data->list->length++;
#define N 3
    if (c != N) {
        fprintf(stderr, "%s(): required %d columns but %d found\n", __FUNCTION__, N, c);
        return EXIT_FAILURE;
    }
#undef N
    return EXIT_SUCCESS;
}

int getPin_callback(void *data, int argc, char **argv, char **azColName) {
    PinData *d = data;
    int c = 0;
    for (int i = 0; i < argc; i++) {
        if (DB_COLUMN_IS("net_id")) {
            d->list->item[d->list->length].net_id = atoi(argv[i]);
            c++;
        } else if (DB_COLUMN_IS("device_id")) {
            d->list->item[d->list->length].device = getDeviceBy_id(atoi(argv[i]), d->device_list);
            c++;
        } else if (DB_COLUMN_IS("id_within_device")) {
            d->list->item[d->list->length].id_dev = atoi(argv[i]);
            c++;
        } else if (DB_COLUMN_IS("mode")) {
            d->list->item[d->list->length].mode = getModeByStr(argv[i]);
            c++;
        } else if (DB_COLUMN_IS("pud")) {
            d->list->item[d->list->length].pud = getPUDByStr(argv[i]);
            c++;
        } else if (DB_COLUMN_IS("rsl")) {
            d->list->item[d->list->length].pwm.rsl = atoi(argv[i]);
            c++;
        } else if (DB_COLUMN_IS("pwm_period_sec")) {
            d->list->item[d->list->length].pwm.period.tv_sec = atoi(argv[i]);
            c++;
        } else if (DB_COLUMN_IS("pwm_period_nsec")) {
            d->list->item[d->list->length].pwm.period.tv_nsec = atoi(argv[i]);
            c++;
        } else if (DB_COLUMN_IS("secure_timeout_sec")) {
            d->list->item[d->list->length].secure_out.timeout.tv_sec = atoi(argv[i]);
            d->list->item[d->list->length].secure_out.timeout.tv_nsec = 0;
            d->list->item[d->list->length].secure_out.tmr.ready = 0;
            c++;
        } else if (DB_COLUMN_IS("secure_duty_cycle")) {
            d->list->item[d->list->length].secure_out.duty_cycle = atoi(argv[i]);
            c++;
        } else if (DB_COLUMN_IS("secure_enable")) {
            d->list->item[d->list->length].secure_out.enable = atoi(argv[i]);
            c++;
        } else {
#ifdef MODE_DEBUG
            fprintf(stderr, "%s(): unknown column\n", __FUNCTION__);
#endif
        }
    }
    d->list->length++;
#define N 11
    if (c != N) {
        fprintf(stderr, "%s(): required %d columns but %d found\n", __FUNCTION__, N, c);
        return EXIT_FAILURE;
    }
#undef N
    return EXIT_SUCCESS;
}

int initDevPin(DeviceList *dl, PinList *pl, int device_count_max, int pin_count_max, const char *db_path) {
    sqlite3 *db;
    if (!db_openR(db_path, &db)) {
        return 0;
    }
    int n = 0;
    char q[LINE_SIZE];
    if (!db_getInt(&n, db, "select count(*) from device")) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): failed\n", __FUNCTION__);
#endif
        return 0;
    }
    if (n <= 0 || n > device_count_max) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): device count: %d, but should be >0 and <=%d\n", __FUNCTION__, n, device_count_max);
#endif
        sqlite3_close(db);
        return 0;
    }
    if (!initDeviceList(dl, n)) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): failed\n", __FUNCTION__);
#endif
        return 0;
    }
    memset(dl->item, 0, n * sizeof *(dl->item));
    DeviceData data = {.list = dl};
    snprintf(q, sizeof q, "select id, i2c_path, i2c_addr from device limit %d", device_count_max);
    if (!db_exec(db, q, getDevice_callback, &data)) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): failed\n", __FUNCTION__);
#endif
        sqlite3_close(db);
        return 0;
    }
    if (dl->length != n) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): device found: %u, but expected: %d\n", __FUNCTION__, dl->length, n);
#endif
        sqlite3_close(db);
        return 0;
    }

    n = 0;
    if (!db_getInt(&n, db, "select count(*) from pin")) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): failed\n", __FUNCTION__);
#endif
        return 0;
    }
    if (n <= 0 || n > pin_count_max) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): device pin count: %d, but should be >0 and <=%d\n", __FUNCTION__, n, pin_count_max);
#endif
        sqlite3_close(db);
        return 0;
    }
    if (!initPinList(pl, n)) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): failed\n", __FUNCTION__);
#endif
        return 0;
    }
    memset(pl->item, 0, n * sizeof *(pl->item));
    PinData datap = {.list = pl, .device_list = dl};
    snprintf(q, sizeof q, PIN_QUERY_STR, pin_count_max);
    if (!db_exec(db, q, getPin_callback, &datap)) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): failed\n", __FUNCTION__);
#endif
        sqlite3_close(db);
        return 0;
    }
    sqlite3_close(db);
    if (pl->length != n) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): pins found: %u, but expected: %d\n", __FUNCTION__, dl->length, n);
#endif
        return 0;
    }
    return 1;
}