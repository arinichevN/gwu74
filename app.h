
#ifndef GWU74_APP_H
#define GWU74_APP_H


#define APP_NAME_STR TOSTRING(gwu74)

#ifdef MODE_FULL
#define CONF_DIR "/etc/controller/" APP_NAME_STR "/"
#endif
#ifndef MODE_FULL
#define CONF_DIR "./"
#endif
#define CONFIG_FILE "" CONF_DIR "config.tsv"

extern int initData();

extern void freeData();

extern void freeApp();

extern void exit_nicely();

extern int readSettings();

extern void serverRun(int *state, int init_state);

extern void *threadFunction(void *arg);

extern int createThread_ctl();

extern int initApp();

#endif

