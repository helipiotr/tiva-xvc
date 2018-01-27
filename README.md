# Xilinx Virtual Cable Protocol implementation for Tiva TM4C1294

This implementation is intended for programming Xilinx FPGAs via JTAG, through an ethernet cable.

# Use

Download the repository. Use third_party to patch the proper files in TIVA lwip port. The implementation of sys_arch.c from TI does not have three necessary functions (link error) and message boxes and semaphores are not being fully deleted (not removed from the list, see the creation process). Connect the following pins on the board:


TDI: PA4
TDO: PE5
TCK: PA2
TMS: PA5
TDI frame select: PA3 
TDO frame select: PB4
TDO in clock: PB5

Frame select signals are necessary for the operation. After the build, connect an ethernet cable and make sure the network accepts IP 192.168.0.45 or change enet_lwip.c appropriately. Use connection_test.py to see whether the system works correctly. It has not been tested in an actual connection with an FPGA: issues are welcome.
