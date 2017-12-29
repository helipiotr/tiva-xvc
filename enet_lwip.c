//*****************************************************************************
//
// enet_lwip.c - Sample WebServer Application using lwIP.
//
// Copyright (c) 2013-2017 Texas Instruments Incorporated.  All rights reserved.
// Software License Agreement
// 
// Texas Instruments (TI) is supplying this software for use solely and
// exclusively on TI's microcontroller products. The software is owned by
// TI and/or its suppliers, and is protected under applicable copyright
// laws. You may not combine this software with "viral" open-source
// software in order to form a larger program.
// 
// THIS SOFTWARE IS PROVIDED "AS IS" AND WITH ALL FAULTS.
// NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT
// NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. TI SHALL NOT, UNDER ANY
// CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
// DAMAGES, FOR ANY REASON WHATSOEVER.
// 
// This is part of revision 2.1.4.178 of the EK-TM4C1294XL Firmware Package.
//
//*****************************************************************************

#include <stdbool.h>
#include <stdint.h>
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "driverlib/flash.h"
#include "driverlib/interrupt.h"
#include "driverlib/gpio.h"
#include "driverlib/rom_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/systick.h"
#include "utils/locator.h"
#include "utils/lwiplib.h"
#include "utils/ustdlib.h"
#include "utils/uartstdio.h"
#include "httpserver_raw/httpd.h" //currently just for test purpurses
#include "drivers/pinout.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "enet_lwip.h"

//*****************************************************************************
//
//! \addtogroup example_list
//! <h1>Ethernet with lwIP (enet_lwip)</h1>
//!
//! This example application demonstrates the operation of the Tiva
//! Ethernet controller using the lwIP TCP/IP Stack.  DHCP is used to obtain
//! an Ethernet address.  If DHCP times out without obtaining an address,
//! AutoIP will be used to obtain a link-local address.  The address that is
//! selected will be shown on the UART.
//!
//! UART0, connected to the ICDI virtual COM port and running at 115,200,
//! 8-N-1, is used to display messages from this application. Use the
//! following command to re-build the any file system files that change.
//!
//!     ../../../../tools/bin/makefsfile -i fs -o enet_fsdata.h -r -h -q
//!
//! For additional details on lwIP, refer to the lwIP web page at:
//! http://savannah.nongnu.org/projects/lwip/
//
//*****************************************************************************

// The current IP address.
uint32_t g_ui32IPAddress;

// The system clock frequency.
uint32_t g_ui32SysClock;

// Volatile global flag to manage LED blinking, since it is used in interrupt
// and main application.  The LED blinks at the rate of BLINK_TASK_PERIOD_MS.
volatile bool g_bLED;

// A handle by which this task and others can refer to this task.
xTaskHandle g_xBlinkHandle;

// A handle by which this task and others can refer to this task.
xTaskHandle g_xXVCHandle;

// The error routine that is called if the driver library encounters an error.
#ifdef DEBUG
void
__error__(char *pcFilename, uint32_t ui32Line)
{
}
#endif

// Counter value used by the FreeRTOS run time stats feature.
// http://www.freertos.org/rtos-run-time-stats.html
volatile unsigned long g_vulRunTimeStatsCountValue;


// This hook is called by FreeRTOS when an stack overflow error is detected.
void
vApplicationStackOverflowHook(xTaskHandle *pxTask, char *pcTaskName)
{
    UARTprintf("FreeRTOS stack overflow error. \n");
    //
    // This function can not return, so loop forever.  Interrupts are disabled
    // on entry to this function, so no processor interrupts will interrupt
    // this loop.
    //
    while(1)
    {
    }
}

// Display an lwIP type IP Address.
void
DisplayIPAddress(uint32_t ui32Addr)
{
    char pcBuf[16];

    //
    // Convert the IP Address into a string.
    //
    usprintf(pcBuf, "%d.%d.%d.%d", ui32Addr & 0xff, (ui32Addr >> 8) & 0xff,
            (ui32Addr >> 16) & 0xff, (ui32Addr >> 24) & 0xff);

    //
    // Display the string.
    //
    UARTprintf(pcBuf);
}


// Required by lwIP library to support any host-related timer functions.
void
lwIPHostTimerHandler(void)
{
    uint32_t ui32NewIPAddress;

    //
    // Get the current IP address.
    //
    ui32NewIPAddress = lwIPLocalIPAddrGet();

    //
    // See if the IP address has changed.
    //
    if(ui32NewIPAddress != g_ui32IPAddress)
    {
        //
        // See if there is an IP address assigned.
        //
        if(ui32NewIPAddress == 0xffffffff)
        {
            //
            // Indicate that there is no link.
            //
            UARTprintf("Waiting for link.\n");
        }
        else if(ui32NewIPAddress == 0)
        {
            //
            // There is no IP address, so indicate that the DHCP process is
            // running.
            //
            UARTprintf("Waiting for IP address.\n");
        }
        else
        {
            //
            // Display the new IP address.
            //
            UARTprintf("IP Address: ");
            DisplayIPAddress(ui32NewIPAddress);
            UARTprintf("\nOpen a browser and enter the IP address.\n");
        }

        //
        // Save the new IP address.
        //
        g_ui32IPAddress = ui32NewIPAddress;
    }

    //
    // If there is not an IP address.
    //
    if((ui32NewIPAddress == 0) || (ui32NewIPAddress == 0xffffffff))
    {
        //
        // Do nothing and keep waiting.
        //
    }
}

static int sread(int fd, void *target, int len) {
   unsigned char *t = target;
   while (len) {
      int r = lwip_read(fd, t, len);
      if (r <= 0)
         return r;
      t += r;
      len -= r;
   }
   return 1;
}

int handle_data(int fd /*, volatile jtag_t* ptr*/) {

    const char xvcInfo[] = "xvcServer_v1.0:2048\n";

    do {
        char cmd[16];
        unsigned char buffer[2048], result[1024];
        memset(cmd, 0, 16);

        if (sread(fd, cmd, 2) != 1)
            return 1;

        if (memcmp(cmd, "ge", 2) == 0) {
            if (sread(fd, cmd, 6) != 1)
                return 1;
            memcpy(result, xvcInfo, strlen(xvcInfo));
            if (lwip_write(fd, result, strlen(xvcInfo)) != strlen(xvcInfo)) {
                UARTprintf("Socket write error. \n");
                return 1;
            }

            UARTprintf("Received command: 'getinfo'\n");
            UARTprintf("\t Replied with xvcServer_v1.0:2048 \n");

            break;
        } else if (memcmp(cmd, "se", 2) == 0) {
            if (sread(fd, cmd, 9) != 1)
                return 1;
            memcpy(result, cmd + 5, 4);
            if (lwip_write(fd, result, 4) != 4) {
                UARTprintf("Socket write error. \n");
                return 1;
            }

            UARTprintf("Received command: 'settck'\n");
            //printf("\t Replied with '%.*s'\n\n", 4, cmd + 5);

            break;
        } else if (memcmp(cmd, "sh", 2) == 0) {
            if (sread(fd, cmd, 4) != 1)
                return 1;

            UARTprintf("Received command: 'shift'\n");

        } else {

            UARTprintf("Invalid cmd \n");
            return 1;
        }

        int len;
        if (sread(fd, &len, 4) != 1) {
            UARTprintf("Reading length failed\n");
            return 1;
        }

        int nr_bytes = (len + 7) / 8;
        if (nr_bytes * 2 > sizeof(buffer)) {
            UARTprintf("Buffer size exceeded\n");
            return 1;
        }

        if (sread(fd, buffer, nr_bytes * 2) != 1) {
            UARTprintf("Reading data failed\n");
            return 1;
        }
        memset(result, 0, nr_bytes);

        /*printf("\tNumber of Bits  : %d\n", len);
        printf("\tNumber of Bytes : %d \n", nr_bytes);
        printf("\n");*/

            int bytesLeft = nr_bytes;
            int bitsLeft = len;
            int byteIndex = 0;
            int tdi, tms, tdo;

            while (bytesLeft > 0) {
                tms = 0;
                tdi = 0;
                tdo = 0;
                if (bytesLeft >= 4) {
                    memcpy(&tms, &buffer[byteIndex], 4);
                    memcpy(&tdi, &buffer[byteIndex + nr_bytes], 4);

                    //TODO JTAG integration
                    //ptr->length_offset = 32;
                    //ptr->tms_offset = tms;
                    //ptr->tdi_offset = tdi;
                    //ptr->ctrl_offset = 0x01;

                    /* Switch this to interrupt in next revision */
                    while ( 0 /*ptr->ctrl_offset*/)
                        {
            }

                    //tdo = ptr->tdo_offset;
                    memcpy(&result[byteIndex], &tdo, 4);

                    bytesLeft -= 4;
                    bitsLeft -= 32;
                    byteIndex += 4;

                    /*if (verbose) {
                        printf("LEN : 0x%08x\n", 32);
                        printf("TMS : 0x%08x\n", tms);
                        printf("TDI : 0x%08x\n", tdi);
                        printf("TDO : 0x%08x\n", tdo);
                    }*/

                } else {
                    memcpy(&tms, &buffer[byteIndex], bytesLeft);
                    memcpy(&tdi, &buffer[byteIndex + nr_bytes], bytesLeft);

                    //TODO JTAG integration
                    //ptr->length_offset = bitsLeft;
                    //ptr->tms_offset = tms;
                    //ptr->tdi_offset = tdi;
                    //ptr->ctrl_offset = 0x01;
                    /* Switch this to interrupt in next revision */
                    while ( 0 /*ptr->ctrl_offset */)
                        {
            }

                    //tdo = ptr->tdo_offset;

                    memcpy(&result[byteIndex], &tdo, bytesLeft);

                    /*if (verbose) {
                        printf("LEN : 0x%08x\n", 32);
                        printf("TMS : 0x%08x\n", tms);
                        printf("TDI : 0x%08x\n", tdi);
                        printf("TDO : 0x%08x\n", tdo);
                    }*/
                    break;
                }
            }
        if (lwip_write(fd, result, nr_bytes) != nr_bytes) {
            UARTprintf("Socket write error");
            return 1;
        }

    } while (1);
    /* Note: Need to fix JTAG state updates, until then no exit is allowed */
    return 0;
}

// LED blink task.
static void
BlinkTask(void)
{
    portTickType xLastWakeTime;
    for(;;){
    //
    // Wait for the required amount of time to check back.
    //
    vTaskDelayUntil(&xLastWakeTime, BLINK_TASK_PERIOD_MS /
                    portTICK_RATE_MS);

    //g_bLED = (g_bLED ? false : true);
    if (g_bLED){
        g_bLED = false;
    }else{
        g_bLED = true;
    }
    MAP_GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_1,
                             (MAP_GPIOPinRead(GPIO_PORTN_BASE, GPIO_PIN_1) ^
                              GPIO_PIN_1));
    }
}

// XVC Task
static void
XVCTask(void)
{

    int i;
    int s; //socket
    int addressCorrect = 0;
    portTickType xLastWakeTime;

    struct sockaddr_in address;



    while ( g_ui32IPAddress == 0 )
    {
        //suspend, IP address may be set at the wake up
        vTaskDelayUntil(&xLastWakeTime, XVC_TASK_PERIOD_MS /
                        portTICK_RATE_MS);
    }

    //lwip as there is apparently some conflict in the name space
    s = lwip_socket(AF_INET, SOCK_STREAM, 0);

    if (s < 0) {
        UARTprintf("Socket creation error. \n");
        for(;;){} //infinite loop
    }

    i = 1;
    lwip_setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &i, sizeof i);

    address.sin_addr.s_addr = g_ui32IPAddress;
    address.sin_port = htons(2542); //XVC uses port 2542
    address.sin_family = AF_INET;

    if (lwip_bind(s, (struct sockaddr*) &address, sizeof(address)) < 0) {
        UARTprintf("Socket bind error. \n");
        for(;;){} //infinite loop
    }

    if (lwip_listen(s, 0) < 0) {
        UARTprintf("Socket listen error. \n");
        for(;;){} //infinite loop
    }

    fd_set conn;
    int maxfd = 0;

    FD_ZERO(&conn);
    FD_SET(s, &conn);

    maxfd = s;


    for(;;)
    {
        fd_set read = conn, except = conn;
        int fd;

        if (lwip_select(maxfd + 1, &read, 0, &except, 0) < 0) {
            UARTprintf("Select error. \n");
           break;
        }

        for (fd = 0; fd <= maxfd; ++fd) {
           if (FD_ISSET(fd, &read)) {
              if (fd == s) {
                 int newfd;
                 socklen_t nsize = sizeof(address);

                 newfd = lwip_accept(s, (struct sockaddr*) &address, &nsize);

                 //UARTprintf("connection accepted - fd %d\n", newfd);
                 if (newfd < 0) {
                     UARTprintf("Accept error. \n");
                 } else {
                     UARTprintf("setting TCP_NODELAY to 1\n");
                    int flag = 1;
                    int optResult = lwip_setsockopt(newfd,
                                               IPPROTO_TCP,
                                               TCP_NODELAY,
                                               (char *)&flag,
                                               sizeof(int));
                    if (optResult < 0)
                        UARTprintf("TCP_NODELAY error \n");
                    if (newfd > maxfd) {
                       maxfd = newfd;
                    }
                    FD_SET(newfd, &conn);
                 }
              }
              else if ( 1 /*handle_data(fd,ptr)*/) { //TODO: write data handling function

                 //UARTprintf("connection closed - fd %d\n \n",fd);
                 close(fd);
                 FD_CLR(fd, &conn);
              }
           }
           else if (FD_ISSET(fd, &except)) {
              //UARTprintf("connection aborted - fd %d\n", fd);
              close(fd);
              FD_CLR(fd, &conn);
              if (fd == s)
                 break;
           }
        }

        //vTaskDelayUntil(&xLastWakeTime, XVC_TASK_PERIOD_MS /
        //                        portTICK_RATE_MS);
    }

}


// This example demonstrates the use of the Ethernet Controller.
int
main(void)
{
    uint32_t ui32User0, ui32User1;
    uint8_t pui8MACArray[8];

    //
    // Make sure the main oscillator is enabled because this is required by
    // the PHY.  The system must have a 25MHz crystal attached to the OSC
    // pins. The SYSCTL_MOSC_HIGHFREQ parameter is used when the crystal
    // frequency is 10MHz or higher.
    //
    SysCtlMOSCConfigSet(SYSCTL_MOSC_HIGHFREQ);

    //
    // Run from the PLL at 120 MHz.
    //
    g_ui32SysClock = MAP_SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ |
                                             SYSCTL_OSC_MAIN |
                                             SYSCTL_USE_PLL |
                                             SYSCTL_CFG_VCO_480), 120000000);

    //
    // Configure the device pins.
    //
    PinoutSet(true, false);

    //
    // Configure UART.
    //
    UARTStdioConfig(0, 115200, g_ui32SysClock);

    //
    // Clear the terminal and print banner.
    //
    UARTprintf("\033[2J\033[H");
    UARTprintf("Ethernet lwIP example\n\n");

    //
    // Configure Port N1 for as an output for the animation LED.
    //
    MAP_GPIOPinTypeGPIOOutput(GPIO_PORTN_BASE, GPIO_PIN_1);

    //
    // Initialize LED to OFF (0)
    //
    MAP_GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_1, ~GPIO_PIN_1);

    //
    // Configure SysTick for a periodic interrupt.
    //
    MAP_SysTickPeriodSet(g_ui32SysClock / SYSTICKHZ);
    MAP_SysTickEnable();
    MAP_SysTickIntEnable();

    //
    // Configure the hardware MAC address for Ethernet Controller filtering of
    // incoming packets.  The MAC address will be stored in the non-volatile
    // USER0 and USER1 registers.
    //
    MAP_FlashUserGet(&ui32User0, &ui32User1);
    if((ui32User0 == 0xffffffff) || (ui32User1 == 0xffffffff))
    {
        //
        // We should never get here.  This is an error if the MAC address has
        // not been programmed into the device.  Exit the program.
        // Let the user know there is no MAC address
        //
        UARTprintf("No MAC programmed!\n");
        while(1)
        {
        }
    }

    //
    // Tell the user what we are doing just now.
    //
    UARTprintf("Waiting for IP.\n");

    //
    // Convert the 24/24 split MAC address from NV ram into a 32/16 split MAC
    // address needed to program the hardware registers, then program the MAC
    // address into the Ethernet Controller registers.
    //
    pui8MACArray[0] = ((ui32User0 >>  0) & 0xff);
    pui8MACArray[1] = ((ui32User0 >>  8) & 0xff);
    pui8MACArray[2] = ((ui32User0 >> 16) & 0xff);
    pui8MACArray[3] = ((ui32User1 >>  0) & 0xff);
    pui8MACArray[4] = ((ui32User1 >>  8) & 0xff);
    pui8MACArray[5] = ((ui32User1 >> 16) & 0xff);

    //
    // Initialize the lwIP library, using DHCP / static.
    //
    #if !(LWIP_DHCP || LWIP_AUTOIP)
    struct ip_addr temp_ip;
    struct ip_addr temp_mask;
    IP4_ADDR(&temp_ip,45,0,168,192);
    IP4_ADDR(&temp_mask,0,255,255,255);

    //faster for debugging
    lwIPInit(g_ui32SysClock, pui8MACArray, temp_ip.addr , temp_mask.addr , 0, IPADDR_USE_STATIC);
    #else
    lwIPInit(g_ui32SysClock, pui8MACArray, 0, 0, 0, IPADDR_USE_DHCP);
    #endif

    //
    // Setup the device locator service.
    //
    LocatorInit();
    LocatorMACAddrSet(pui8MACArray);
    LocatorAppTitleSet("EK-TM4C1294XL enet_io");

    //
    // Initialize a sample httpd server.
    //
    httpd_init(); //currently just for test purpurses

    //
    // Set the interrupt priorities.  We set the SysTick interrupt to a higher
    // priority than the Ethernet interrupt to ensure that the file system
    // tick is processed if SysTick occurs while the Ethernet handler is being
    // processed.  This is very likely since all the TCP/IP and HTTP work is
    // done in the context of the Ethernet interrupt.
    //
    MAP_IntPrioritySet(INT_EMAC0, ETHERNET_INT_PRIORITY);
    MAP_IntPrioritySet(FAULT_SYSTICK, SYSTICK_INT_PRIORITY);

    // Create blink LED task
    g_bLED = false;
    xTaskCreate(BlinkTask, (const portCHAR *)"Blink",
                       BLINK_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY +
                       PRIORITY_BLINK_TASK, g_xBlinkHandle);

    //g_ui32IPAddress = 0; //set to a known state before task
    // Create XVC task
    xTaskCreate(XVCTask, (const portCHAR *)"XVC",
                XVC_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY +
                       PRIORITY_XVC_TASK, g_xXVCHandle);

    //
    // Start the scheduler.  This should not return.
    //

    vTaskStartScheduler();

    //
    // In case the scheduler returns for some reason, print an error and loop
    // forever.
    //
    UARTprintf("RTOS scheduler returned unexpectedly.\n");
    while(1)
    {
        //
        // Do Nothing.
        //
    }

}
