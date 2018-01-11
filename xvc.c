/*
 * xvc.c
 *
 *  Created on: 30.12.2017
 *      Author: Piotr Wierzba
 */

#include "xvc.h"
#include "utils/lwiplib.h"
#include "utils/uartstdio.h"
#include "string.h"

#include "FreeRTOS.h"
#include "task.h"

#include "jtag.h"

// A handle by which blink task and others can refer to the task.
extern xTaskHandle g_xXVCHandle;

// The current IP address.
extern uint32_t g_ui32IPAddress;

// The system clock frequency.
extern uint32_t g_ui32SysClock;

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

            //jtag_setclk(result[0]); << to be tested

            if (lwip_write(fd, result, 4) != 4) {
                UARTprintf("Socket write error. \n");
                return 1;
            }

            UARTprintf("Received command: 'settck', functionality currently unimplemented.\n");
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
                    memcpy(&tms, &buffer[byteIndex], 4);
                    memcpy(&tdi, &buffer[byteIndex + nr_bytes], 4);

                    jtag_send_receive(tdi, tms, &tdo);

                    memcpy(&result[byteIndex], &tdo, 1);

                    bytesLeft -= 1;
                    bitsLeft -= 8;
                    byteIndex += 1;

            }
        if (lwip_write(fd, result, nr_bytes) != nr_bytes) {
            UARTprintf("Socket write error");
            return 1;
        }

    } while (1);
    /* Note: Need to fix JTAG state updates, until then no exit is allowed */
    return 0;
}


// XVC Task
void XVCTask(void)
{

    int i;
    int s; //socket
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
                 lwip_close(fd);
                 FD_CLR(fd, &conn);
              }
           }
           else if (FD_ISSET(fd, &except)) {
              //UARTprintf("connection aborted - fd %d\n", fd);
              lwip_close(fd);
              FD_CLR(fd, &conn);
              if (fd == s)
                 break;
           }
        }

        //vTaskDelayUntil(&xLastWakeTime, XVC_TASK_PERIOD_MS /
        //                        portTICK_RATE_MS);
    }

}
