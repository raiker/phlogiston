arm-none-eabi-g++ -mcpu=arm1176jzf-s -g -fpic -ffreestanding -c boot.S -o boot.o
arm-none-eabi-g++ -mcpu=arm1176jzf-s -g -fpic -ffreestanding -c interrupts.S -o interrupts.o
arm-none-eabi-g++ -mcpu=arm1176jzf-s -g -fpic -ffreestanding -fno-exceptions -std=c++14 -O2 -Wall -Wextra -c kmain.cc -o kmain.o
arm-none-eabi-g++ -mcpu=arm1176jzf-s -g -fpic -ffreestanding -fno-exceptions -std=c++14 -O2 -Wall -Wextra -c mmio.cc -o mmio.o
arm-none-eabi-g++ -mcpu=arm1176jzf-s -g -fpic -ffreestanding -fno-exceptions -std=c++14 -O2 -Wall -Wextra -c uart.cc -o uart.o
arm-none-eabi-g++ -mcpu=arm1176jzf-s -g -fpic -ffreestanding -fno-exceptions -std=c++14 -O2 -Wall -Wextra -c atags.cc -o atags.o
arm-none-eabi-g++ -mcpu=arm1176jzf-s -g -fpic -ffreestanding -fno-exceptions -std=c++14 -O2 -Wall -Wextra -c utility.cc -o utility.o
arm-none-eabi-g++ -g -T link.ld -o phlogiston.elf -ffreestanding -O2 boot.o interrupts.o utility.o mmio.o uart.o atags.o kmain.o -nostdlib -lgcc
arm-none-eabi-objcopy phlogiston.elf -O binary phlogiston.bin
arm-none-eabi-objcopy --only-keep-debug phlogiston.elf phlogiston.sym
