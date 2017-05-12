
#include "main.h"

void printData(DeviceList *dl, PinList *pl) {
    int i = 0;
    char q[LINE_SIZE];
    uint8_t crc = 0;
    snprintf(q, sizeof q, "CONFIG_FILE: %s\n", CONFIG_FILE);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "port: %d\n", sock_port);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "pid_path: %s\n", pid_path);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "sock_buf_size: %d\n", sock_buf_size);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "cycle_duration sec: %ld\n", cycle_duration.tv_sec);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "cycle_duration nsec: %ld\n", cycle_duration.tv_nsec);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "i2c_path: %s\n", i2c_path);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "device_name: %s\n", device_name);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "app_state: %s\n", getAppState(app_state));
    sendStr(q, &crc);
    snprintf(q, sizeof q, "PID: %d\n", proc_id);
    sendStr(q, &crc);
    sendStr("+-----------------------+\n", &crc);
    sendStr("|        device         |\n", &crc);
    sendStr("+-----------+-----------+\n", &crc);
    sendStr("|    id     |  fd_i2c   |\n", &crc);
    sendStr("+-----------+-----------+\n", &crc);
    for (i = 0; i < dl->length; i++) {
        snprintf(q, sizeof q, "|%11d|%11d|\n", dl->item[i].id, dl->item[i].fd_i2c);
        sendStr(q, &crc);
    }
    sendStr("+-----------+-----------+\n", &crc);

    for (i = 0; i < dl->length; i++) {
        snprintf(q, sizeof q, "device id: %d, read1: %x, read2: %x\n", dl->item[i].id, dl->item[i].read1, dl->item[i].read2);
        sendStr(q, &crc);
        sendStr("+---+-----------+-----------+\n", &crc);
        sendStr("| id| new_data  | old_data  |\n", &crc);
        sendStr("+---+-----------+-----------+\n", &crc);
        snprintf(q, sizeof q, "| 1 |%11x|%11x|\n", dl->item[i].new_data1, dl->item[i].old_data1);
        sendStr(q, &crc);
        snprintf(q, sizeof q, "| 2 |%11x|%11x|\n", dl->item[i].new_data2, dl->item[i].old_data2);
        sendStr(q, &crc);
        snprintf(q, sizeof q, "| 3 |%11x|%11x|\n", dl->item[i].new_data3, dl->item[i].old_data3);
        sendStr(q, &crc);
        snprintf(q, sizeof q, "| 4 |%11x|%11x|\n", dl->item[i].new_data4, dl->item[i].old_data4);
        sendStr(q, &crc);
        snprintf(q, sizeof q, "| 5 |%11x|%11x|\n", dl->item[i].new_data5, dl->item[i].old_data5);
        sendStr(q, &crc);
        snprintf(q, sizeof q, "| 6 |%11x|%11x|\n", dl->item[i].new_data6, dl->item[i].old_data6);
        sendStr(q, &crc);
        sendStr("+---+-----------+-----------+\n", &crc);
    }

    sendStr("+-----------------------------------------------------------------------------------------------------------------------------------+\n", &crc);
    sendStr("|                                                                pin                                                                |\n", &crc);
    sendStr("+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+\n", &crc);
    sendStr("|  net_id   |  device   |   id_dev  |   mode    |   PUD     |    out    |   value   | duty_cycle| period s  | period ns |    rsl    |\n", &crc);
    sendStr("+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+\n", &crc);
    for (i = 0; i < pl->length; i++) {
        snprintf(q, sizeof q, "|%11d|%11p|%11d|%11.11s|%11.11s|%11d|%11d|%11d|%11ld|%11ld|%11u|\n",
                pl->item[i].net_id,
                pl->item[i].device,
                pl->item[i].id_dev,
                getPinModeStr(pl->item[i].mode),
                getPinPUDStr(pl->item[i].pud),
                pl->item[i].out,
                pl->item[i].value,
                pl->item[i].duty_cycle,
                pl->item[i].pwm.period.tv_sec,
                pl->item[i].pwm.period.tv_nsec,
                pl->item[i].pwm.rsl
                );
        sendStr(q, &crc);
    }
    sendStr("+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+\n", &crc);
    sendFooter(crc);
}

void printDevice(DeviceList *list) {
    size_t i;
    puts("+-----------------------+");
    puts("|        device         |");
    puts("+-----------+-----------+");
    puts("|    id     |  fd_i2c   |");
    puts("+-----------+-----------+");
    for (i = 0; i < list->length; i++) {
        printf("|%11d|%11d|\n",
                list->item[i].id,
                list->item[i].fd_i2c
                );
    }
    puts("+-----------+-----------+");
}

void printPin(PinList *list) {
    size_t i;
    puts("+-----------------------------------------------------------------------------------------------------------+");
    puts("|                                                      pin                                                  |");
    puts("+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+");
    puts("|   net_id  |  device   |   id_dev  |   mode    |    PUD    |   value   | duty_cycle| period s  |    rsl    |");
    puts("+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+");
    for (i = 0; i < list->length; i++) {
        printf("|%11d|%11p|%11d|%11.11s|%11.11s|%11d|%11d|%11ld|%11u|\n",
                list->item[i].net_id,
                list->item[i].device,
                list->item[i].id_dev,
                getPinModeStr(list->item[i].mode),
                getPinPUDStr(list->item[i].pud),
                list->item[i].value,
                list->item[i].duty_cycle,
                list->item[i].pwm.period.tv_sec,
                list->item[i].pwm.rsl
                );
    }
    puts("+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+");
}

void printHelp() {
    char q[LINE_SIZE];
    uint8_t crc = 0;
    sendStr("COMMAND LIST\n", &crc);
    snprintf(q, sizeof q, "%c\tput process into active mode; process will read configuration\n", ACP_CMD_APP_START);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tput process into standby mode; all running programs will be stopped\n", ACP_CMD_APP_STOP);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tfirst stop and then start process\n", ACP_CMD_APP_RESET);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tterminate process\n", ACP_CMD_APP_EXIT);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tget state of process; response: B - process is in active mode, I - process is in standby mode\n", ACP_CMD_APP_PING);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tget some variable's values; response will be packed into multiple packets\n", ACP_CMD_APP_PRINT);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tget this help; response will be packed into multiple packets\n", ACP_CMD_APP_HELP);
    sendStr(q, &crc);

    snprintf(q, sizeof q, "%c\tget input pin state; pin net_id expected if '.' quantifier is used; response: id\\tin\n", ACP_CMD_GET_INT);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tget pin data; pin net_id expected if '.' quantifier is used; response: id\\tmode\\tpud\\tPWMPeriodSec\\tPWMPeriodNsec\n", ACP_CMD_GWU74_GET_DATA);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tget pin output value; pin net_id expected if '.' quantifier is used; response: id\\tout\n", ACP_CMD_GWU74_GET_OUT);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tset output pin state; pin net_id expected if '.' quantifier is used\n", ACP_CMD_SET_INT);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tset duty cycle; dutyCycle expected; pin net_id expected if '.' quantifier is used\n", ACP_CMD_SET_DUTY_CYCLE_PWM);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tset PWM period (usec); PWMPeriod expected; pin net_id expected if '.' quantifier is used\n", ACP_CMD_SET_PWM_PERIOD);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tset PWM resolution (maximum duty cycle); PWMPeriod expected; pin net_id expected if '.' quantifier is used\n", ACP_CMD_GWU74_SET_RSL);
    sendStr(q, &crc);
    sendFooter(crc);
}






