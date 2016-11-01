
#ifndef MAIN_H
#define MAIN_H

#include "lib/db.h"
#include "lib/app.c"
#include "lib/gpio.h"
#include "lib/i2c.h"
#include "lib/acp/main.h"
#include "lib/acp/app.h"
#include "lib/acp/cmd.h"
#include "lib/acp/pcf8574.h"
#include "lib/udp.h"
#include "lib/pm.h"
#include "lib/pwm.h"


#define APP_NAME ("gwu74")

#define MAX_PIN_NUM 128 //16 devices with 8 pins in each8
#define GROUP_STR_BUFSIZE 256
#define PIN_SDA 3 //physical pin number
#define PIN_SCL 5 //physical pin number
#define OCC_PIN_NUM 2 //PIN_SDA and PIN_SCL
#define DEVICE_PIN_NUM 8
#define PIN_BROADCAST 0xff
#define GROUP_BROADCAST 0xff

#define MODE_IN_STR "in"
#define MODE_OUT_STR "out"
#define MODE_OUT_CMN_STR "cmn"
#define MODE_OUT_PWM_STR "pwm"
#define MODE_OUT_PM_STR "pm"

//pin mode
#define MODE_IN 0
#define MODE_OUT_CMN 1
#define MODE_OUT_PWM 2
#define MODE_OUT_PM 3


#ifndef MODE_DEBUG
#define CONF_FILE ("/etc/controller/gwu74.conf")
#endif
#ifdef MODE_DEBUG
#define CONF_FILE ("main.conf")
#endif


typedef struct {
    int addr;
    int fd;
    int new_data; //data we want to write into chip
    int old_data; //copy of chip data
} Device;

typedef struct {
    Device *device;
    size_t length;
} DeviceList;

typedef struct {
    int id; //unique
    int net_id;//this id will be used for interprocess communication
    int id_dev; //0-7 id within its device
    Device *device;
    int value; //LOW || HIGH
    int duty_cycle;
    int mode; //MDOE_IN || MODE_OUT_CMN || MDOE_OUT_PWM || MODE_OUT_PM
    int mode_sv; //saved
    struct timespec pwm_period_sv; //saved
    PWM pwm;
} Pin;

typedef struct {
    Pin *item;
    size_t length;
} PinList;

typedef struct {
    Pin **item;
    size_t length;
} PinPList;

typedef struct {
    Device **item;
    size_t length;
} DevicePList;

typedef struct {
    pthread_t *thread;
    size_t length;
} ThreadList;

typedef struct {
    int thread_attr_init;
    int thread_attr_setdetachstate;
    int thread_create;
} ThreadInitSuccess;

typedef struct {
    pthread_attr_t thread_attr;
    pthread_t thread;
    char cmd;
    char qfr;
    I1List i1l;
    I2List i2l;
    ThreadInitSuccess init_success;
    int on;
    struct timespec cycle_duration;
} ThreadData;


extern int readSettings() ;

extern int getModeByStr(const char *str) ;

extern int initAppData(DeviceList *dl, PinList *pl) ;

extern int checkAppData(DeviceList *dl, PinList *pl) ;

extern void serverRun(int *state, int init_state) ;

extern void updateOutSafe(PinList *list) ;

extern void writeDevice(DeviceList *list);

extern void updatePinMode(PinList *list);

extern void *threadFunction(void *arg) ;

extern int createThread(ThreadData * td, size_t buf_len) ;

extern void initApp() ;

extern void initData() ;

extern void freeThread() ;

extern void freeAppData() ;

extern void freeData() ;

extern void freeApp() ;

extern void exit_nicely() ;

extern void exit_nicely_e(char *s) ;

#endif /* MAIN_H */

