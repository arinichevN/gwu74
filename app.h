
#ifndef GWU74_APP_H
#define GWU74_APP_H


#define APP_NAME_STR TOSTRING(gwu74)

#ifndef MODE_DEBUG
#define CONF_FILE ("/etc/controller/gwu74.conf")
#endif
#ifdef MODE_DEBUG
#define CONF_FILE ("main.conf")
#endif

#endif

