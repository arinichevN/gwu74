
#ifndef GWU74_H
#define GWU74_H

#include "lib/dbl.h"
#include "lib/app.h"
#include "lib/configl.h"

#include "lib/acp/main.h"
#include "lib/acp/app.h"
#include "lib/acp/cmd.h"
#include "lib/acp/gwu74.h"
#include "lib/acp/lck.h"
#include "lib/acp/prog.h"
#include "lib/udp.h"
#include "lib/tsv.h"

#include "app.h"

#include "device/main.h"
#include "device/idle.h"
#include "device/native.h"
#include "device/mcp23008.h"
#include "device/mcp23017.h"
#include "device/pcf8574.h"

#define WAIT_RESP_TIMEOUT 3
#define LOCK_COM_INTERVAL 1000000U

extern int initDevice(DeviceList *dl, PinList *pl, char *device);

extern void updateOutSafe(PinList *list);

#endif

