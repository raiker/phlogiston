#include "common.h"
#include "uart.h"
#include "atags.h"
#include "page_alloc.h"
#include "panic.h"
#include "elf_loader.h"
#include "pagetable.h"
#include "runtime_tests.h"

//#define RUN_TESTS

extern uint32_t _binary_kernel_stripped_elf_start;
  
extern "C" /* Use C linkage for kernel_main. */
void loader_main(uint32_t r0, uint32_t r1, void * atags, uint32_t cpsr_saved)
{
	uart_init();
	uart_puts("Hello, kernel World!\r\n");
	
	uart_puthex(r0); uart_puts("\r\n");
	uart_puthex(r1); uart_puts("\r\n");
	uart_puthex((uint32_t)atags); uart_puts("\r\n");
	uart_puthex(cpsr_saved); uart_puts("\r\n");
	
	atags::debug_atags(atags);
	
	MemRange system_memory = atags::get_mem_range(atags);
	
	if (system_memory.start != 0){
		panic(PanicCodes::NonZeroBase);
	}
	
	//set up temporary stacks
	asm volatile(
		"cpsid aif\n" //disable interrupts
		"cps #0x11\n" //FIQ mode
		"ldr sp, =0x00808000\n"
		"cps #0x12\n" //IRQ mode
		"ldr sp, =0x00810000\n"
		"cps #0x17\n" //Abort mode
		"ldr sp, =0x00818000\n"
		"cps #0x1b\n" //Undefined mode
		"ldr sp, =0x00820000\n"
		"cps #0x13\n" //Supervisor mode
		);
	
	//set up exception vector
	asm volatile(
		"ldr r0, =ivt_start\n"
		"mcr p15, 0, r0, c12, c0, 0"
		: : : "r0");
	
	asm volatile("svc #0");
	
	uart_puts("Enabling interrupts\r\n");
	
	//enable interrupts
	asm volatile("cpsie aif");
	
	page_alloc::init(system_memory.size);
	
	uintptr_t fiq_stack = page_alloc::alloc(1) + PAGE_SIZE;
	uintptr_t irq_stack = page_alloc::alloc(1) + PAGE_SIZE;
	uintptr_t abort_stack = page_alloc::alloc(1) + PAGE_SIZE;
	uintptr_t undef_stack = page_alloc::alloc(1) + PAGE_SIZE;
	
	asm volatile(
		"cpsid aif\n" //disable interrupts
		"cps #0x11\n" //FIQ mode
		"mov sp, %[fiq_stack]\n"
		"cps #0x12\n" //IRQ mode
		"mov sp, %[irq_stack]\n"
		"cps #0x17\n" //Abort mode
		"mov sp, %[abort_stack]\n"
		"cps #0x1b\n" //Undefined mode
		"mov sp, %[undef_stack]\n"
		"cps #0x13\n" //Supervisor mode
		"cpsie aif" //re-enable interrupts
		: :
		[fiq_stack] "r" (fiq_stack),
		[irq_stack] "r" (irq_stack),
		[abort_stack] "r" (abort_stack),
		[undef_stack] "r" (undef_stack)
		);
	
#ifdef RUN_TESTS
	if (test_pagetables()){
		uart_puts("All tests passed\r\n");
	} else {
		uart_puts("Some tests failed\r\n");
	}
#else
	
	PageTable supervisor_table(true);
	
	if (!load_elf((void*)&_binary_kernel_stripped_elf_start, supervisor_table)){
		uart_puts("Failed to load kernel\r\n");
		panic(PanicCodes::AssertionFailure);
	}
		
	//supervisor_table.print_table_info();

	uart_putline();
	
	//page table to handle identity-mapping the physical memory space
	PageTable identity_overlay(false, false);
	
	//map all of ram
	uint32_t nsections = get_num_allocation_units(system_memory.size, AllocationGranularity::Section);	
	if (!identity_overlay.reserve(0x00000000, nsections, AllocationGranularity::Section).is_success){
		uart_puts("Failed to reserve identity memory\r\n");
		panic(PanicCodes::AssertionFailure);
	}
	
	if (!identity_overlay.map(0x00000000, 0x00000000, nsections, AllocationGranularity::Section)){
		uart_puts("Failed to map identity\r\n");
		panic(PanicCodes::AssertionFailure);
	}
	
	//map mmio
	nsections = 16;
	if (!identity_overlay.reserve(0x20000000, nsections, AllocationGranularity::Section).is_success){
		uart_puts("Failed to reserve identity memory\r\n");
		panic(PanicCodes::AssertionFailure);
	}
	
	if (!identity_overlay.map(0x20000000, 0x20000000, nsections, AllocationGranularity::Section)){
		uart_puts("Failed to map identity\r\n");
		panic(PanicCodes::AssertionFailure);
	}
	
	//identity_overlay.print_table_info();
	
	PagingManager::SetLowerPageTable(identity_overlay);
	PagingManager::SetUpperPageTable(supervisor_table);
	PagingManager::SetPagingMode(true, true);
	PagingManager::EnablePaging();
	
	uart_puthex(*((uint32_t*)0x80000000));
	uart_putline();
#endif
	while ( true )
		uart_putc(uart_getc());
}

extern "C"
uint32_t svc_entry(uint32_t a, uint32_t b, uint32_t c, uint32_t d){
	uart_puts("System call!\r\n");
	uart_puts("a: "); uart_puthex(a); uart_puts("\r\n");
	uart_puts("b: "); uart_puthex(b); uart_puts("\r\n");
	uart_puts("c: "); uart_puthex(c); uart_puts("\r\n");
	uart_puts("d: "); uart_puthex(d); uart_puts("\r\n");
	return 0;
}

extern "C"
void undef_instr_handler(uint32_t saved_pc){
	uart_puts("Undefined instruction at ");
	uart_puthex(saved_pc);
	uart_puts("\r\n");
}

extern "C"
void prefetch_abort_handler(uint32_t saved_pc){
	uart_puts("Prefetch abort at ");
	uart_puthex(saved_pc);
	uart_puts("\r\n");
}

extern "C"
void data_abort_handler(uint32_t saved_pc){
	uart_puts("Data abort at ");
	uart_puthex(saved_pc);
	uart_puts("\r\n");
}

extern "C"
void irq_handler(){
	uart_puts("IRQ triggered\r\n");
}

extern "C"
void fiq_handler(){
	uart_puts("FIQ triggered\r\n");
}

