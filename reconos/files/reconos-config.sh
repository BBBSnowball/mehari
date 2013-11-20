#!/bin/sh

xil_tools="/opt/Xilinx/14.6"
gnutools="$xil_tools/ISE_DS/EDK/gnu/arm/lin/bin/arm-xilinx-linux-gnueabi-"
libc_dir="$(dirname "$gnutools")/../arm-xilinx-linux-gnueabi/libc"
reconos_arch="zynq"
reconos_os="linux"
reconos_mmu=true
