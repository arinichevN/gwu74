
#include "main.h"
#define PIN_QUERY_STR "select net_id, device_id, id_within_device, mode, pud, pwm_resolution, pwm_period_sec, pwm_period_nsec, pwm_duty_cycle_min_sec, pwm_duty_cycle_min_nsec, pwm_duty_cycle_max_sec, pwm_duty_cycle_max_nsec, secure_timeout_sec, secure_duty_cycle, secure_enable from pin limit %d"

typedef struct {
    PinList *list;
    DeviceList *device_list;
} PinData;

int getDevice_callback ( void *data, int argc, char **argv, char **azColName ) {
    DeviceList *list = data;
    int c = 0;
    for ( int i = 0; i < argc; i++ ) {
        if ( DB_COLUMN_IS ( "id" ) ) {
            list->item[list->length].id = DB_CVI;
            c++;
        } else if ( DB_COLUMN_IS ( "i2c_addr" ) ) {
            list->item[list->length].i2c_addr = DB_CVI;
            // list->item[list->length].i2c_fd = I2COpen(i2c_path, DB_CVI);
            c++;
        } else if ( DB_COLUMN_IS ( "i2c_path" ) ) {
            size_t sz=0;
            size_t sz1=strlen ( DB_COLUMN_VALUE );
            size_t sz2=sizeof list->item[list->length].i2c_path;
            if ( sz1 < sz2 ) {
                sz=sz1;
            } else {
                sz=sz2;
            }
            memcpy ( list->item[list->length].i2c_path, DB_COLUMN_VALUE, sz );
            c++;
        } else {
#ifdef MODE_DEBUG
            fprintf ( stderr, "%s(): unknown column\n", F );
#endif
            c++;
        }
    }
    list->length++;
#define N 3
    if ( c != N ) {
        fprintf ( stderr, "%s(): required %d columns but %d found\n", F, N, c );
        return EXIT_FAILURE;
    }
#undef N
    return EXIT_SUCCESS;
}

int getPin_callback ( void *data, int argc, char **argv, char **azColName ) {
    PinData *d = data;
    int c = 0;
    for ( int i = 0; i < argc; i++ ) {
        if ( DB_COLUMN_IS ( "net_id" ) ) {
            d->list->item[d->list->length].net_id = DB_CVI;
            c++;
        } else if ( DB_COLUMN_IS ( "device_id" ) ) {
            GET_DEVICE ( d->list->item[d->list->length].device, d->device_list, DB_CVI )
            c++;
        } else if ( DB_COLUMN_IS ( "id_within_device" ) ) {
            d->list->item[d->list->length].id_dev = DB_CVI;
            c++;
        } else if ( DB_COLUMN_IS ( "mode" ) ) {
            d->list->item[d->list->length].mode = getModeByStr ( DB_COLUMN_VALUE );
            c++;
        } else if ( DB_COLUMN_IS ( "pud" ) ) {
            d->list->item[d->list->length].pud = getPUDByStr ( DB_COLUMN_VALUE );
            c++;
        } else if ( DB_COLUMN_IS ( "pwm_resolution" ) ) {
            d->list->item[d->list->length].pwm.resolution = DB_CVI;
            c++;
        } else if ( DB_COLUMN_IS ( "pwm_period_sec" ) ) {
            d->list->item[d->list->length].pwm.period.tv_sec = DB_CVI;
            c++;
        } else if ( DB_COLUMN_IS ( "pwm_period_nsec" ) ) {
            d->list->item[d->list->length].pwm.period.tv_nsec = DB_CVI;
            c++;
        } else if ( DB_COLUMN_IS ( "pwm_duty_cycle_min_sec" ) ) {
            d->list->item[d->list->length].pwm.duty_cycle_min.tv_sec = DB_CVI;
            c++;
        } else if ( DB_COLUMN_IS ( "pwm_duty_cycle_min_nsec" ) ) {
            d->list->item[d->list->length].pwm.duty_cycle_min.tv_nsec = DB_CVI;
            c++;
        } else if ( DB_COLUMN_IS ( "pwm_duty_cycle_max_sec" ) ) {
            d->list->item[d->list->length].pwm.duty_cycle_max.tv_sec = DB_CVI;
            c++;
        } else if ( DB_COLUMN_IS ( "pwm_duty_cycle_max_nsec" ) ) {
            d->list->item[d->list->length].pwm.duty_cycle_max.tv_nsec = DB_CVI;
            c++;
        } else if ( DB_COLUMN_IS ( "secure_timeout_sec" ) ) {
            d->list->item[d->list->length].secure_out.timeout.tv_sec = DB_CVI;
            d->list->item[d->list->length].secure_out.timeout.tv_nsec = 0;
            d->list->item[d->list->length].secure_out.tmr.ready = 0;
            d->list->item[d->list->length].secure_out.error_code = &d->list->item[d->list->length].error_code;
            c++;
        } else if ( DB_COLUMN_IS ( "secure_duty_cycle" ) ) {
            d->list->item[d->list->length].secure_out.duty_cycle = DB_CVI;
            c++;
        } else if ( DB_COLUMN_IS ( "secure_enable" ) ) {
            d->list->item[d->list->length].secure_out.enable = DB_CVI;
            c++;
        } else {
#ifdef MODE_DEBUG
            fprintf ( stderr, "%s(): unknown column\n", F );
#endif
            c++;
        }
    }
    if ( !pwm_check ( &d->list->item[d->list->length].pwm ) ) {
        return EXIT_FAILURE;
    }
    pwm_init ( &d->list->item[d->list->length].pwm );
    d->list->item[d->list->length].error_code=0x0u;
    d->list->length++;
#define N 15
    if ( c != N ) {
        fprintf ( stderr, "%s(): required %d columns but %d found\n", F, N, c );
        return EXIT_FAILURE;
    }
#undef N
    return EXIT_SUCCESS;
}

int initDevPin ( DeviceList *dl, PinList *pl, int device_count_max, int pin_count_max, const char *db_path ) {
    sqlite3 *db;
    if ( !db_openR ( db_path, &db ) ) {
        return 0;
    }
    int n = 0;
    char q[LINE_SIZE*2];
    if ( !db_getInt ( &n, db, "select count(*) from device" ) ) {
#ifdef MODE_DEBUG
        fprintf ( stderr, "%s(): failed\n", F );
#endif
        return 0;
    }
    if ( n <= 0 || n > device_count_max ) {
#ifdef MODE_DEBUG
        fprintf ( stderr, "%s(): device count: %d, but should be >0 and <=%d\n", F, n, device_count_max );
#endif
        sqlite3_close ( db );
        return 0;
    }
    RESIZE_M_LIST ( dl, n );
    if ( dl->max_length != n ) {
#ifdef MODE_DEBUG
        fprintf ( stderr, "%s(): device_list resize failed\n", F );
#endif
        return 0;
    }
    NULL_LIST ( dl );
    snprintf ( q, sizeof q, "select id, i2c_path, i2c_addr from device limit %d", device_count_max );
    if ( !db_exec ( db, q, getDevice_callback, dl ) ) {
        sqlite3_close ( db );
        return 0;
    }
    if ( dl->length != dl->max_length ) {
#ifdef MODE_DEBUG
        fprintf ( stderr, "%s(): device found: %u, but expected: %d\n", F, dl->length, dl->max_length );
#endif
        sqlite3_close ( db );
        return 0;
    }

    n = 0;
    if ( !db_getInt ( &n, db, "select count(*) from pin" ) ) {
#ifdef MODE_DEBUG
        fprintf ( stderr, "%s(): failed\n", F );
#endif
        return 0;
    }
    if ( n <= 0 || n > pin_count_max ) {
#ifdef MODE_DEBUG
        fprintf ( stderr, "%s(): device pin count: %d, but should be >0 and <=%d\n", F, n, pin_count_max );
#endif
        sqlite3_close ( db );
        return 0;
    }
    RESIZE_M_LIST ( pl, n );
    if ( pl->max_length != n ) {
#ifdef MODE_DEBUG
        fprintf ( stderr, "%s(): pin_list resize failed\n", F );
#endif
        return 0;
    }
    NULL_LIST ( pl );
    PinData datap = {.list = pl, .device_list = dl};
    snprintf ( q, sizeof q, PIN_QUERY_STR, pin_count_max );
    if ( !db_exec ( db, q, getPin_callback, &datap ) ) {
        sqlite3_close ( db );
        return 0;
    }
    sqlite3_close ( db );
    if ( pl->length != pl->max_length ) {
#ifdef MODE_DEBUG
        fprintf ( stderr, "%s(): pins found: %u, but expected: %d\n", F, pl->length, pl->max_length );
#endif
        return 0;
    }
    return 1;
}
