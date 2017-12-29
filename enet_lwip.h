/*
 * enet_lwip.h
 *
 *  Created on: 28.12.2017
 *      Author: Piotr Wierzba
 */

#ifndef ENET_LWIP_H_
#define ENET_LWIP_H_

// Defines for setting up the system clock.
#define SYSTICKHZ               100
#define SYSTICKMS               (1000 / SYSTICKHZ)

// Interrupt priority definitions.  The top 3 bits of these values are
// significant with lower values indicating higher priority interrupts.
#define SYSTICK_INT_PRIORITY    0x80
#define ETHERNET_INT_PRIORITY   0xC0

//
// The stack size for the BLINK task.
//
#define BLINK_TASK_STACK_SIZE        1024         // Stack size in words


// Period in milliseconds to determine time between how often run the task.
// This determines how often we blink with an LED
#define BLINK_TASK_PERIOD_MS         500        // periodic rate of the task

// A handle by which blink task and others can refer to the task.
extern xTaskHandle g_xBlinkHandle;

#define PRIORITY_BLINK_TASK       13

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


#endif /* ENET_LWIP_H_ */
