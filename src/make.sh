ASMFLAGS="-mcpu=arm1176jzf-s -fpic -ffreestanding"
CXXFLAGS="-fno-exceptions -fno-unwind-tables -fno-rtti -g -std=c++17 -Wall -Wextra" #-O3

arm-none-eabi-g++ $ASMFLAGS -c boot.S -o build/boot.o
arm-none-eabi-g++ $ASMFLAGS -c interrupts.S -o build/interrupts.o
arm-none-eabi-g++ $ASMFLAGS $CXXFLAGS -c loader_main.cc -o build/loader_main.o
arm-none-eabi-g++ $ASMFLAGS $CXXFLAGS -c mmio.cc -o build/mmio.o
arm-none-eabi-g++ $ASMFLAGS $CXXFLAGS -c uart.cc -o build/uart.o
arm-none-eabi-g++ $ASMFLAGS $CXXFLAGS -c atags.cc -o build/atags.o
arm-none-eabi-g++ $ASMFLAGS $CXXFLAGS -c utility.cc -o build/utility.o
arm-none-eabi-g++ $ASMFLAGS $CXXFLAGS -c page_alloc.cc -o build/page_alloc.o
arm-none-eabi-g++ $ASMFLAGS $CXXFLAGS -c panic.cc -o build/panic.o
arm-none-eabi-g++ $ASMFLAGS $CXXFLAGS -c elf_loader.cc -o build/elf_loader.o
arm-none-eabi-g++ $ASMFLAGS $CXXFLAGS -c kernel_entry.cc -o build/kernel_entry.o
arm-none-eabi-g++ $ASMFLAGS $CXXFLAGS -c spinlock.cc -o build/spinlock.o
arm-none-eabi-g++ $ASMFLAGS $CXXFLAGS -c pagetable.cc -o build/pagetable.o
arm-none-eabi-g++ $ASMFLAGS $CXXFLAGS -c pagetable_tests.cc -o build/pagetable_tests.o

arm-none-eabi-g++ -g -T phlogiston_link.ld -o kernel.elf -flto -fpic -ffreestanding -O2 build/utility.o build/mmio.o build/uart.o build/panic.o build/kernel_entry.o -nostdlib -lgcc

arm-none-eabi-objcopy --only-keep-debug kernel.elf kernel.sym
arm-none-eabi-objcopy -S kernel.elf kernel-stripped.elf
arm-none-eabi-objcopy -I binary -O elf32-littlearm -B arm kernel-stripped.elf kernel-binary.o

arm-none-eabi-g++ -g -T loader_link.ld -o loader.elf -flto -fpic -ffreestanding -O2 build/boot.o build/interrupts.o build/utility.o build/mmio.o build/uart.o build/atags.o build/page_alloc.o build/panic.o build/elf_loader.o build/loader_main.o build/spinlock.o build/pagetable.o build/pagetable_tests.o kernel-binary.o -nostdlib -lgcc

arm-none-eabi-objcopy loader.elf -O binary phlogiston.bin

