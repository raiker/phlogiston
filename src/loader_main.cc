#include "common.h"
#include "uart.h"
#include "atags.h"
#include "page_alloc.h"
#include "panic.h"
#include "elf_loader.h"

extern uint32_t _binary_kernel_stripped_elf_start;
  
extern "C" /* Use C linkage for kernel_main. */
void loader_main(uint32_t r0, uint32_t r1, void * atags, uint32_t cpsr_saved)
{
	//(void) r0;
	//(void) r1;
	//(void) atags;
 
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
	
	//set up stacks
	asm volatile(
		"mrs r4, cpsr\n"
		"bic r4, r4, #0x1f\n" //clear out mode bits
		"orr r5, r4, #0x11\n" //FIQ mode
		"msr cpsr_c, r5\n" //switch to mode
		"ldr sp, =0x00808000\n"
		"orr r5, r4, #0x12\n" //IRQ mode
		"msr cpsr_c, r5\n" //switch to mode
		"ldr sp, =0x00810000\n"
		"orr r5, r4, #0x17\n" //Abort mode
		"msr cpsr_c, r5\n" //switch to mode
		"ldr sp, =0x00818000\n"
		"orr r5, r4, #0x1b\n" //Undefined mode
		"msr cpsr_c, r5\n" //switch to mode
		"ldr sp, =0x00820000\n"
		"orr r5, r4, #0x13\n" //Supervisor mode
		"msr cpsr_c, r5" //switch to mode
		: : : "r4", "r5");
	
	//set up exception vector
	asm volatile(
		"ldr r0, =ivt_start\n"
		"mcr p15, 0, r0, c12, c0, 0"
		: : : "r0");
	
	asm volatile("svc #0");
	
	uart_puts("Enabling interrupts\r\n");
	
	asm volatile(
		"mrs r4, cpsr\n"
		"bic r4, r4, #0xc0\n" //clear out interrupt bits
		"msr cpsr_c, r4" //enable interrupts
		: : : "r4");
	
	page_alloc::init(system_memory.size);
	
	/*page_alloc::MemStats stats = page_alloc::get_mem_stats();
	uart_puts("Total mem: "); uart_puthex(stats.totalmem); uart_puts("\r\n");
	uart_puts("Free mem:  "); uart_puthex(stats.freemem); uart_puts("\r\n");
	uart_puts("Used mem:  "); uart_puthex(stats.usedmem); uart_puts("\r\n");
	
	uintptr_t a1 = page_alloc::alloc(1);
	uintptr_t a2 = page_alloc::alloc(4);
	uintptr_t a3 = page_alloc::alloc(256);
	
	uart_puthex(a1); uart_puts("\r\n");
	uart_puthex(a2); uart_puts("\r\n");
	uart_puthex(a3); uart_puts("\r\n");
	
	stats = page_alloc::get_mem_stats();
	uart_puts("Total mem: "); uart_puthex(stats.totalmem); uart_puts("\r\n");
	uart_puts("Free mem:  "); uart_puthex(stats.freemem); uart_puts("\r\n");
	uart_puts("Used mem:  "); uart_puthex(stats.usedmem); uart_puts("\r\n");*/
	
	elf_parse_header((void*)&_binary_kernel_stripped_elf_start);
 
	while ( true )
		uart_putc(uart_getc());
}

extern "C"
uint32_t svc_entry(uint32_t a, uint32_t b, uint32_t c, uint32_t d){
	uart_puts("System call!\r\n");
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
void irq_handler(uint32_t saved_pc){
	uart_puts("IRQ triggered\r\n");
}

extern "C"
void fiq_handler(uint32_t saved_pc){
	uart_puts("FIQ triggered\r\n");
}

