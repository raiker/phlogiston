ASMFLAGS="-mcpu=arm1176jzf-s -fpic -ffreestanding"
CXXFLAGS="-fno-exceptions -g -std=c++17 -O3 -Wall -Wextra"

arm-none-eabi-g++ $ASMFLAGS -c boot.S -o boot.o
arm-none-eabi-g++ $ASMFLAGS -c interrupts.S -o interrupts.o
arm-none-eabi-g++ $ASMFLAGS $CXXFLAGS -c kmain.cc -o kmain.o
arm-none-eabi-g++ $ASMFLAGS $CXXFLAGS -c mmio.cc -o mmio.o
arm-none-eabi-g++ $ASMFLAGS $CXXFLAGS -c uart.cc -o uart.o
arm-none-eabi-g++ $ASMFLAGS $CXXFLAGS -c atags.cc -o atags.o
arm-none-eabi-g++ $ASMFLAGS $CXXFLAGS -c utility.cc -o utility.o
arm-none-eabi-g++ $ASMFLAGS $CXXFLAGS -c page_alloc.cc -o page_alloc.o
arm-none-eabi-g++ $ASMFLAGS $CXXFLAGS -c panic.cc -o panic.o
arm-none-eabi-g++ -g -T link.ld -o phlogiston.elf -flto -fpic -ffreestanding -O2 boot.o interrupts.o utility.o mmio.o uart.o atags.o page_alloc.o panic.o kmain.o -nostdlib -lgcc
arm-none-eabi-objcopy phlogiston.elf -O binary phlogiston.bin
arm-none-eabi-objcopy --only-keep-debug phlogiston.elf phlogiston.sym
