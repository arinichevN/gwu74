
#ifndef GWU74_DEV_H
#define GWU74_DEV_H

#include "../lib/dbl.h"
#include "../lib/gpio.h"
#include "../lib/pm.h"
#include "../lib/pwm.h"

#define DIO_HIGH 1
#define DIO_LOW 0

//pin mode
#define DIO_MODE_IN 0
#define DIO_MODE_OUT 1
#define DIO_MODE_IN_STR "in"
#define DIO_MODE_OUT_STR "out"

//pin pull up/down
#define DIO_PUD_OFF 0
#define DIO_PUD_DOWN 1
#define DIO_PUD_UP 2
#define DIO_PUD_OFF_STR "off"
#define DIO_PUD_DOWN_STR "down"
#define DIO_PUD_UP_STR "up"

#define DEVICE_MCP23008_STR "mcp23008"
#define DEVICE_MCP23017_STR "mcp23017"
#define DEVICE_PCF8574_STR "pcf8574"
#define DEVICE_NATIVE_STR "native"
#define DEVICE_IDLE_STR "idle"

typedef struct {
    int id;
    int fd_i2c;
    int new_data1;
    int old_data1;
    int new_data2;
    int old_data2;
    int new_data3;
    int old_data3;
    int new_data4;
    int old_data4;
    int new_data5;
    int old_data5;
    int new_data6;
    int old_data6;
    int read1;
    int read2;
    Mutex mutex;
} Device;

DEC_LIST(Device)

typedef struct {
    int enable;
   struct timespec timeout;
   int duty_cycle;
   Ton_ts tmr;
   int done;
}DOSecure;

typedef struct {
    int net_id; //this id will be used for interprocess communication
    int id_dev; //id within its device
    Device *device;
    int value; //LOW || HIGH
    int value_state;
    int duty_cycle;
    int mode; //MDOE_IN || MODE_OUT
    int pud;
    PWM pwm;
    DOSecure secure_out;
    int out_pwm; //1-pwm mode
    int out;
    struct timespec tm;
    Mutex mutex;
} Pin;

DEC_LIST(Pin)

extern void (*setMode)(Pin *, int);
extern void (*setPUD)(Pin *, int);
extern void (*setOut)(Pin *, int);
extern void (*getIn)(Pin *);
extern void (*writeDeviceList)(DeviceList *);
extern void (*readDeviceList)(DeviceList *, PinList *);


#endif