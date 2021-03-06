/*
 * jtag.h
 *
 *  Created on: 01.01.2018
 *      Author: Piotr Wierzba
 */

#ifndef JTAG_H_
#define JTAG_H_

#include <stdbool.h>
#include <stdint.h>


void jtag_send_receive(uint32_t tdi, uint32_t tms, uint32_t* tdo);
void jtag_init();
void jtag_setclk(uint32_t jtag_clk_ns);

#endif /* JTAG_H_ */
