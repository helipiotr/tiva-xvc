/*
 * xvc.h
 *
 *  Created on: 30.12.2017
 *      Author: Piotr Wierzba
 */

#ifndef XVC_H_
#define XVC_H_

#include "utils/lwiplib.h"
#include "utils/uartstdio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "string.h"

//
// The stack size for the XVC task.
//
#define XVC_TASK_STACK_SIZE        1024         // Stack size in words

// Period in milliseconds to determine time between how often run the task.
// This determines how often we blink with an LED
#define XVC_TASK_PERIOD_MS         500        // periodic rate of the task

// A handle by which blink task and others can refer to the task.
extern xTaskHandle g_xXVCHandle;

#define PRIORITY_XVC_TASK       12

// The current IP address.
extern uint32_t g_ui32IPAddress;

static int sread(int fd, void *target, int len);

int handle_data(int fd /*, volatile jtag_t* ptr*/);

void XVCTask(void);



#endif /* XVC_H_ */
