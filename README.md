# ppu-driver

## Description

Driver and userland application for a simple Posit Processing Unit described in [federicorossifr/qemu-ppu](https://github.com/federicorossifr/qemu-ppu) in a RISC-V environment.
The repository is structured as follows:
  * ppu.c is the kernel module driver that enables PCI communication with the device. Including instrumentation of DMA and bus-mastering
  * ppu_user.c is the userland space program with an example application
  
## Operations supported
  * Posit from/to float conversion using memory mapped I/O 
  * Batch conversions using the DMA approach
  
## TODOs
  * Implement a request structure to be passed to the kernel module
  * Implement queuing of request with a lock on the resources
  


