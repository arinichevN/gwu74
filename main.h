
#ifndef GWU74_H
#define GWU74_H

#include "lib/db.h"
#include "lib/app.c"
#include "lib/config.c"

#include "lib/acp/main.h"
#include "lib/acp/app.h"
#include "lib/acp/cmd.h"
#include "lib/acp/gwu74.h"
#include "lib/udp.h"


#include "app.h"

#include "device/main.h"
#include "device/idle.h"
#include "device/native.h"
#include "device/mcp23008.h"
#include "device/mcp23017.h"
#include "device/pcf8574.h"

//typedef struct {
//    pthread_t *thread;
//    size_t length;
//} ThreadList;

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
    ThreadInitSuccess init_success;
    int on;
    struct timespec cycle_duration;
} ThreadData;


extern int readSettings();

extern void serverRun(int *state, int init_state);

extern void updateOutSafe(PinList *list);

extern void *threadFunction(void *arg);

extern int createThread(ThreadData * td);

extern void initApp();

extern int initDevice(DeviceList *dl, PinList *pl, char *device);

extern void initData();

extern void freeThread();

extern void freeAppData();

extern void freeData();

extern void freeApp();

extern void exit_nicely();

extern void exit_nicely_e(char *s);



#endif

