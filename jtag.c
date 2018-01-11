/*
 * jtag.c
 *
 *  Created on: 01.01.2018
 *      Author: Piotr Wierzba
 */


#include "jtag.h"
#include "inc/hw_memmap.h"
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/ssi.h"
#include "driverlib/sysctl.h"
#include "driverlib/uart.h"
#include "utils/uartstdio.h"

// The system clock frequency.
extern uint32_t g_ui32SysClock;

//
// This function changes the format of the tdi and tms data, so that the
// bi-SSI is capable of transferring data. Essentially, bi-SSI sends every other
// bit on a different data lane. Hence transformation is needed. Suppose tdi=0xAC, tms = 0xCA:
// i/2 :    7654 3210,  i%2:
// 0xAC = 0b1010 1100   0
//          ^\^\ ^\^\
// 0xCA = 0b1100 1010   1
// We traverse bits, (from left-bottom, up -> right-down -> up etc..) to create two package vectors,
// pkg_0 is created from bits from the right side, pkg_1 from the ones on the left.
// pkg_0 = 0b11100100
// pkg_1 = 0b11011000

//
void jtag_send_receive(uint32_t tdi, uint32_t tms, uint32_t* tdo)
{
    uint32_t pkg_0 = 0;
    uint32_t pkg_1 = 0;
    uint32_t tdo_upper;
    uint32_t tdo_lower;

    int i;

    #pragma UNROLL(10)
    for (i = 0; i < 8; ++i )
    {
        if ( i%2 ) //assign from tdi
        {
            pkg_0 |= ((1 << (i/2)) & tdi) << (i - i/2);
        }
        else //assign from tms
        {
            pkg_0 |= ((1 << (i/2)) & tms) << (i - i/2);
        }

    }

    #pragma UNROLL(10)
    for (i = 8; i < 16; ++i )
    {
        if ( i%2 ) //assign from tdi
        {
            pkg_1 |= ((1 << (i/2)) & tdi) >> (8 - i/2 - i%2);
        }
        else //assign from tms
        {
            pkg_1 |= ((1 << (i/2)) & tms) >> (8 - i/2 - i%2);
        }

    }

    SSIDataPut(SSI0_BASE, pkg_0);

    SSIDataPut(SSI0_BASE, pkg_1);

    while(SSIBusy(SSI0_BASE))
    {
    }

    while(SSIBusy(SSI1_BASE))
    {
    }
    //for JTAG, reception must occur simultaneously to transmission
    SSIDataGet(SSI1_BASE, &tdo_upper);
    SSIDataGet(SSI1_BASE, &tdo_lower);

    *tdo = (tdo_upper << 4) | tdo_lower;

}


// Initialise using clock speed and default ports
void jtag_init(){
    uint32_t temp;

    SysCtlPeripheralEnable(SYSCTL_PERIPH_SSI0); //use SSI0
    SysCtlPeripheralEnable(SYSCTL_PERIPH_SSI1); //use SSI1
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA); //use port A for SSI0
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB); //use port B for SSI1
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE); //use port E for SSI1

    GPIOPinConfigure(GPIO_PA2_SSI0CLK); //clock
    GPIOPinConfigure(GPIO_PB5_SSI1CLK); //clock
    GPIOPinConfigure(GPIO_PA3_SSI0FSS); //frame
    GPIOPinConfigure(GPIO_PB4_SSI1FSS); //frame
    GPIOPinConfigure(GPIO_PA4_SSI0XDAT0); //first send data lane
    GPIOPinConfigure(GPIO_PA5_SSI0XDAT1); //second send data lane
    GPIOPinConfigure(GPIO_PE5_SSI1XDAT1); //receive data lane

    //GPIOs as SSI
    GPIOPinTypeSSI(GPIO_PORTA_BASE, GPIO_PIN_5 | GPIO_PIN_4 | GPIO_PIN_3 |
                   GPIO_PIN_2);
    GPIOPinTypeSSI(GPIO_PORTB_BASE, GPIO_PIN_5 | GPIO_PIN_4);
    GPIOPinTypeSSI(GPIO_PORTE_BASE, GPIO_PIN_5);

    //MOTO mode, tentatively. 1MHz clock speed
    SSIConfigSetExpClk(SSI0_BASE, g_ui32SysClock, SSI_FRF_MOTO_MODE_0,
                       SSI_MODE_MASTER, 1000000, 8);

    // We configure width as four bits, since there is FSS between each four bits
    SSIConfigSetExpClk(SSI1_BASE, g_ui32SysClock, SSI_FRF_MOTO_MODE_0,
                       SSI_MODE_SLAVE, 1000000, 4);

    SSIEnable(SSI0_BASE); //enable
    SSIEnable(SSI1_BASE); //enable

    SSIAdvModeSet(SSI0_BASE, SSI_ADV_MODE_BI_WRITE); //enable bi-ssi

    //remove any unnecessary data from the reception FIFO
    while(SSIDataGetNonBlocking(SSI1_BASE, &temp))
    {
    }

}

// Set clock give a period in nanoseconds
void jtag_setclk(uint32_t jtag_clk_ns)
{
    uint32_t jtag_clk;
    jtag_clk = 1000000000 / jtag_clk_ns;

    if(jtag_clk > 100000000)
        UARTprintf("Warning: JTAG clock set to more than 100 MHz \n");

    SSIConfigSetExpClk(SSI1_BASE, g_ui32SysClock, SSI_FRF_MOTO_MODE_0,
                       SSI_MODE_SLAVE, jtag_clk, 4);
    SSIConfigSetExpClk(SSI0_BASE, g_ui32SysClock, SSI_FRF_MOTO_MODE_0,
                       SSI_MODE_MASTER, jtag_clk, 8);

}
