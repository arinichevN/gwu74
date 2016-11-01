
#include "main.h"

void printData(DeviceList *dl, PinList *pl) {
    int i = 0;
    char q[LINE_SIZE];
    uint8_t crc = 0;
    sendStr("+-----------------------------------------------+\n", &crc);
    sendStr("|                    device                     |\n", &crc);
    sendStr("+-----------+-----------+-----------+-----------+\n", &crc);
    sendStr("|    addr   |    fd     |  new data |  old data |\n", &crc);
    sendStr("+-----------+-----------+-----------+-----------+\n", &crc);
    for (i = 0; i < dl->length; i++) {
        snprintf(q, sizeof q, "|%11d|%11d|%11x|%11x|\n", dl->device[i].addr, dl->device[i].fd, dl->device[i].new_data, dl->device[i].old_data);
        sendStr(q, &crc);
    }
    sendStr("+-----------+-----------+-----------+-----------+\n", &crc);

    sendStr("+-----------------------------------------------------------------------------------+\n", &crc);
    sendStr("|                                        pin                                        |\n", &crc);
    sendStr("+-----------+-----------+-----------+-----------+-----------+-----------+-----------+\n", &crc);
    sendStr("|     id    | dev addr  |    net_id |   mode    |   value   | duty_cycle| pwm_period|\n", &crc);
    sendStr("+-----------+-----------+-----------+-----------+-----------+-----------+-----------+\n", &crc);
    for (i = 0; i < pl->length; i++) {
        snprintf(q, sizeof q, "|%11d|%11d|%11d|%11d|%11d|%11d|%11ld|\n", i, pl->item[i].device->addr, pl->item[i].net_id, pl->item[i].mode, pl->item[i].value, pl->item[i].duty_cycle, pl->item[i].pwm.period.tv_sec);
        sendStr(q, &crc);
    }
    sendStr("+-----------+-----------+-----------+-----------+-----------+-----------+-----------+\n", &crc);
    sendFooter(crc);
}

void printDevice(DeviceList *list) {
    size_t i;
    puts("+-----------------------------------------------+");
    puts("|                    device                     |");
    puts("+-----------+-----------+-----------+-----------+");
    puts("|    addr   |    fd     |  new data |  old data |");
    puts("+-----------+-----------+-----------+-----------+");
    for (i = 0; i < list->length; i++) {
        printf("|%11d|%11d|%11x|%11x|\n",
                list->device[i].addr,
                list->device[i].fd,
                list->device[i].new_data,
                list->device[i].old_data
                );
    }
    puts("+-----------+-----------+-----------+-----------+");
}

void printPin(PinList *list) {
    size_t i;
    puts("+-----------------------------------------------------------------------------------+");
    puts("|                                        pin                                        |");
    puts("+-----------+-----------+-----------+-----------+-----------+-----------+-----------+");
    puts("|     id    | dev addr  |    net_id |   mode    |   value   | duty_cycle| pwm_period|");
    puts("+-----------+-----------+-----------+-----------+-----------+-----------+-----------+");
    for (i = 0; i < list->length; i++) {
        printf("|%11d|%11d|%11d|%11d|%11d|%11d|%11ld|\n",
                list->item[i].id,
                list->item[i].device->addr,
                list->item[i].net_id,
                list->item[i].mode,
                list->item[i].value,
                list->item[i].duty_cycle,
                list->item[i].pwm.period.tv_sec
                );
    }
    puts("+-----------+-----------+-----------+-----------+-----------+-----------+-----------+");
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

    snprintf(q, sizeof q, "%c\tget input pin state; program id expected if '.' quantifier is used\n", ACP_CMD_GET_INT);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tget pin data; program id expected if '.' quantifier is used; response: pinId_mode_PWMPeriod\n", ACP_CMD_PCF8574_GET_DATA);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tset output pin state; program id expected if '.' quantifier is used\n", ACP_CMD_SET_INT);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tset duty cycle; dutyCycle expected; program id expected if '.' quantifier is used\n", ACP_CMD_SET_DUTY_CYCLE_PWM);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tset PWM period; PWMPeriod expected; program id expected if '.' quantifier is used\n", ACP_CMD_SET_PWM_PERIOD);
    sendStr(q, &crc);
    sendFooter(crc);
}






