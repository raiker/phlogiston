ASMFLAGS="-mcpu=arm1176jzf-s -fpic -ffreestanding"
CXXFLAGS="-fno-exceptions -fno-unwind-tables -fno-rtti -fno-delete-null-pointer-checks -g -std=c++17 -Wall -Wextra -Wpedantic -Og "

rm build/*.o
rm bin/*

mkdir -p build
mkdir -p bin

arm-eabi-g++ $ASMFLAGS -c boot.S -o build/boot.o
arm-eabi-g++ $ASMFLAGS -c interrupts.S -o build/interrupts.o
arm-eabi-g++ $ASMFLAGS $CXXFLAGS -c loader_main.cc -o build/loader_main.o
arm-eabi-g++ $ASMFLAGS $CXXFLAGS -c mmio.cc -o build/mmio.o
arm-eabi-g++ $ASMFLAGS $CXXFLAGS -c uart.cc -o build/uart.o
arm-eabi-g++ $ASMFLAGS $CXXFLAGS -c atags.cc -o build/atags.o
arm-eabi-g++ $ASMFLAGS $CXXFLAGS -c utility.cc -o build/utility.o
arm-eabi-g++ $ASMFLAGS $CXXFLAGS -c page_alloc.cc -o build/page_alloc.o
arm-eabi-g++ $ASMFLAGS $CXXFLAGS -c panic.cc -o build/panic.o
arm-eabi-g++ $ASMFLAGS $CXXFLAGS -c elf_loader.cc -o build/elf_loader.o
arm-eabi-g++ $ASMFLAGS $CXXFLAGS -c kernel_entry.cc -o build/kernel_entry.o
arm-eabi-g++ $ASMFLAGS $CXXFLAGS -c spinlock.cc -o build/spinlock.o
arm-eabi-g++ $ASMFLAGS $CXXFLAGS -c pagetable.cc -o build/pagetable.o
arm-eabi-g++ $ASMFLAGS $CXXFLAGS -c pagetable_tests.cc -o build/pagetable_tests.o
arm-eabi-g++ $ASMFLAGS $CXXFLAGS -c process.cc -o build/process.o
arm-eabi-g++ $ASMFLAGS $CXXFLAGS -c sys_timer.cc -o build/sys_timer.o
arm-eabi-g++ $ASMFLAGS $CXXFLAGS -c kernel_interrupt_handlers.cc -o build/kernel_interrupt_handlers.o

arm-eabi-g++ -g -T phlogiston_link.ld -o bin/kernel.elf -flto -fpic -ffreestanding -O2 build/utility.o build/mmio.o build/uart.o build/panic.o build/pagetable.o build/spinlock.o build/page_alloc.o build/process.o build/sys_timer.o build/kernel_interrupt_handlers.o build/interrupts.o build/kernel_entry.o -nostdlib -lgcc

arm-eabi-objcopy --only-keep-debug bin/kernel.elf bin/kernel.sym
arm-eabi-objcopy -S bin/kernel.elf bin/kernel-stripped.elf
arm-eabi-objcopy -I binary -O elf32-littlearm -B arm bin/kernel-stripped.elf bin/kernel-binary.o

arm-eabi-g++ -g -T loader_link.ld -o bin/loader.elf -flto -fpic -ffreestanding -O2 build/boot.o build/interrupts.o build/utility.o build/mmio.o build/uart.o build/atags.o build/page_alloc.o build/panic.o build/elf_loader.o build/loader_main.o build/spinlock.o build/pagetable.o build/pagetable_tests.o bin/kernel-binary.o -nostdlib -lgcc

arm-eabi-objcopy bin/loader.elf -O binary bin/phlogiston.bin

