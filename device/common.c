typedef struct {
    DeviceList *list;
    char * i2c_path;
} DeviceData;

typedef struct {
    PinList *list;
    DeviceList *device_list;
} PinData;

int getDevice_callback(void *data, int argc, char **argv, char **azColName) {
    DeviceData *device_data = data;
    for (int i = 0; i < argc; i++) {
        if (strcmp("id", azColName[i]) == 0) {
            device_data->list->item[device_data->list->length].id = atoi(argv[i]);
        } else if (strcmp("addr_i2c", azColName[i]) == 0) {
            device_data->list->item[device_data->list->length].fd_i2c = I2COpen(device_data->i2c_path, atoi(argv[i]));
        } else {
            putse("getDevice_callback: unknown column\n");
            device_data->list->length++;
            return 1;
        }
    }
    device_data->list->length++;
    return 0;
}

int getPin_callback(void *data, int argc, char **argv, char **azColName) {
    PinData *d = data;
    for (int i = 0; i < argc; i++) {
        if (strcmp("net_id", azColName[i]) == 0) {
            d->list->item[d->list->length].net_id = atoi(argv[i]);
        } else if (strcmp("device_id", azColName[i]) == 0) {
            d->list->item[d->list->length].device = getDeviceBy_id(atoi(argv[i]), d->device_list);
        } else if (strcmp("id_within_device", azColName[i]) == 0) {
            d->list->item[d->list->length].id_dev = atoi(argv[i]);
        } else if (strcmp("mode", azColName[i]) == 0) {
            d->list->item[d->list->length].mode = getModeByStr(argv[i]);
        } else if (strcmp("pud", azColName[i]) == 0) {
            d->list->item[d->list->length].pud = getPUDByStr(argv[i]);
        } else if (strcmp("rsl", azColName[i]) == 0) {
            d->list->item[d->list->length].pwm.rsl = atoi(argv[i]);
        } else if (strcmp("pwm_period_sec", azColName[i]) == 0) {
            d->list->item[d->list->length].pwm.period.tv_sec = atoi(argv[i]);
        } else if (strcmp("pwm_period_nsec", azColName[i]) == 0) {
            d->list->item[d->list->length].pwm.period.tv_nsec = atoi(argv[i]);
        } else {
            putse("getPin_callback: unknown column\n");
            d->list->length++;
            return 1;
        }
    }
    d->list->length++;
    return 0;
}