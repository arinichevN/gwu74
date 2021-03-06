#include "main.h"

void printData ( ACPResponse *response ) {
    DeviceList *dl = &device_list;
    PinList *pl = &pin_list;
    char q[LINE_SIZE];
    snprintf ( q, sizeof q, "CONFIG_FILE: %s\n", CONFIG_FILE );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "port: %d\n", sock_port );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "cycle_duration sec: %ld\n", cycle_duration.tv_sec );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "cycle_duration nsec: %ld\n", cycle_duration.tv_nsec );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "device_name: %s\n", device_name );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "app_state: %s\n", getAppState ( app_state ) );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "PID: %d\n", getpid() );

    SEND_STR ( q )
    SEND_STR ( "+----------------------------------------------------+\n" )
    SEND_STR ( "|                         device                     |\n" )
    SEND_STR ( "+-----------+----------------+-----------+-----------+\n" )
    SEND_STR ( "|    id     |     i2c_path   | i2c_addr  |  i2c_fd   |\n" )
    SEND_STR ( "+-----------+----------------+-----------+-----------+\n" )
    FORLISTP ( dl, i ) {
        snprintf ( q, sizeof q, "|%11d|%16s|%11d|%11d|\n",
                   dl->item[i].id,
                   dl->item[i].i2c_path,
                   dl->item[i].i2c_addr,
                   dl->item[i].i2c_fd
                 );
        SEND_STR ( q )
    }

    SEND_STR ( "+-----------+----------------+-----------+-----------+\n" )

    FORLISTP ( dl, i ) {
        snprintf ( q, sizeof q, "device id: %d, read1: %x, read2: %x\n", dl->item[i].id, dl->item[i].read1, dl->item[i].read2 );
        SEND_STR ( q )
        SEND_STR ( "+---+-----------+-----------+\n" )
        SEND_STR ( "| id| new_data  | old_data  |\n" )
        SEND_STR ( "+---+-----------+-----------+\n" )
        snprintf ( q, sizeof q, "| 1 |%11x|%11x|\n", dl->item[i].new_data1, dl->item[i].old_data1 );
        SEND_STR ( q )
        snprintf ( q, sizeof q, "| 2 |%11x|%11x|\n", dl->item[i].new_data2, dl->item[i].old_data2 );
        SEND_STR ( q )
        snprintf ( q, sizeof q, "| 3 |%11x|%11x|\n", dl->item[i].new_data3, dl->item[i].old_data3 );
        SEND_STR ( q )
        snprintf ( q, sizeof q, "| 4 |%11x|%11x|\n", dl->item[i].new_data4, dl->item[i].old_data4 );
        SEND_STR ( q )
        snprintf ( q, sizeof q, "| 5 |%11x|%11x|\n", dl->item[i].new_data5, dl->item[i].old_data5 );
        SEND_STR ( q )
        snprintf ( q, sizeof q, "| 6 |%11x|%11x|\n", dl->item[i].new_data6, dl->item[i].old_data6 );
        SEND_STR ( q )
        SEND_STR ( "+---+-----------+-----------+\n" )
    }

    SEND_STR ( "+-----------------------------------------------------------------------------------------------------------------------------------+\n" )
    SEND_STR ( "|                                                                pin                                                                |\n" )
    SEND_STR ( "+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+\n" )
    SEND_STR ( "|  net_id   |  device   |   id_dev  |   mode    |   PUD     |    out    |   value   | duty_cycle| period s  | period ns |    rsl    |\n" )
    SEND_STR ( "+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+\n" )

    FORLISTP ( pl, i ) {
        snprintf ( q, sizeof q, "|%11d|%11p|%11d|%11.11s|%11.11s|%11d|%11d|%11d|%11ld|%11ld|%11u|\n",
                   pl->item[i].net_id,
                   ( void * ) pl->item[i].device,
                   pl->item[i].id_dev,
                   getPinModeStr ( pl->item[i].mode ),
                   getPinPUDStr ( pl->item[i].pud ),
                   pl->item[i].out,
                   pl->item[i].value,
                   pl->item[i].duty_cycle,
                   pl->item[i].pwm.period.tv_sec,
                   pl->item[i].pwm.period.tv_nsec,
                   pl->item[i].pwm.resolution
                 );
        SEND_STR ( q )
    }

    SEND_STR ( "+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+\n" )

    SEND_STR ( "+-----------------------------------------------------------+\n" )
    SEND_STR ( "|                       pin secure                          |\n" )
    SEND_STR ( "+-----------+-----------+-----------+-----------+-----------+\n" )
    SEND_STR ( "|  net_id   | timeout_s |time_rest_s|duty_cycle |   enable  |\n" )
    SEND_STR ( "+-----------+-----------+-----------+-----------+-----------+\n" )
    FORLISTP ( pl, i ) {
        struct timespec tm_rest = getTimeRestTmr ( pl->item[i].secure_out.timeout, pl->item[i].secure_out.tmr );
        snprintf ( q, sizeof q, "|%11d|%11ld|%11ld|%11d|%11d|\n",
                   pl->item[i].net_id,
                   pl->item[i].secure_out.timeout.tv_sec,
                   tm_rest.tv_sec,
                   pl->item[i].secure_out.duty_cycle,
                   pl->item[i].secure_out.enable
                 );
        SEND_STR ( q )
    }
    SEND_STR_L ( "+-----------+-----------+-----------+-----------+-----------+\n" )

}

void printHelp ( ACPResponse *response ) {
    char q[LINE_SIZE];
    SEND_STR ( "COMMAND LIST\n" )
    snprintf ( q, sizeof q, "%s\tput process into active mode; process will read configuration\n", ACP_CMD_APP_START );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "%s\tput process into standby mode; all running programs will be stopped\n", ACP_CMD_APP_STOP );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "%s\tfirst stop and then start process\n", ACP_CMD_APP_RESET );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "%s\tterminate process\n", ACP_CMD_APP_EXIT );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "%s\tget state of process; response: B - process is in active mode, I - process is in standby mode\n", ACP_CMD_APP_PING );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "%s\tget some variable's values; response will be packed into multiple packets\n", ACP_CMD_APP_PRINT );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "%s\tget this help; response will be packed into multiple packets\n", ACP_CMD_APP_HELP );
    SEND_STR ( q )

    snprintf ( q, sizeof q, "%s\tget get channel error code; pin net_id expected;\n", ACP_CMD_PROG_GET_ERROR );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "%s\tget input pin state; pin net_id expected; response: net_id\\tstate\\tttimeSec\\ttimeNsec\\tvalid\n", ACP_CMD_GET_FTS );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "%s\tget pin data; pin net_id expected; response: id\\tmode\\tpud\\tPWMPeriodSec\\tPWMPeriodNsec\n", ACP_CMD_PROG_GET_DATA_INIT );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "%s\tget pin output value; pin net_id expected; response: id\\tout\n", ACP_CMD_GWU74_GET_OUT );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "%s\tset output pin state; pin net_id expected\n", ACP_CMD_SET_INT );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "%s\tset duty cycle; dutyCycle expected; pin net_id expected\n", ACP_CMD_SET_FLOAT );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "%s\tset PWM period (usec); PWM_period expected; pin net_id expected\n", ACP_CMD_SET_PWM_PERIOD );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "%s\tset PWM duty cycle min (usec); duty_cycle_min expected; pin net_id expected\n", ACP_CMD_SET_PWM_DUTY_CYCLE_MIN );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "%s\tset PWM duty cycle max (usec); duty_cycle_max expected; pin net_id expected\n", ACP_CMD_SET_PWM_DUTY_CYCLE_MAX );
    SEND_STR ( q )

    snprintf ( q, sizeof q, "%s\tset PWM resolution (maximum duty cycle); resolution expected; pin net_id expected\n", ACP_CMD_SET_PWM_RESOLUTION );
    SEND_STR_L ( q )
}






