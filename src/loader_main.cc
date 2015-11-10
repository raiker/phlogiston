#include "common.h"
#include "uart.h"
#include "atags.h"
#include "page_alloc.h"
#include "panic.h"
#include "elf_loader.h"
#include "pagetable.h"

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
	
	/*for (uint32_t i = 0; i < 10000000; i++){
		uart_puthex(i);
		uart_puts("\r\n");
		page_alloc::alloc(256);
	}*/
	
	PrePagingPageTable table(true);
	
	//table.print_table_info();
	
	table.reserve_commit(52000, AllocationGranularity::Page);
	table.reserve_commit(1234, AllocationGranularity::Page);
	table.reserve_commit(1234, AllocationGranularity::Section);
	table.reserve_commit(1234, AllocationGranularity::Section);
	table.reserve_commit(20000000, AllocationGranularity::Supersection);
	
	table.print_table_info();
	
	uart_putline();
	
	page_alloc::MemStats stats = page_alloc::get_mem_stats();
	uart_puts("Total mem: "); uart_puthex(stats.totalmem); uart_putline();
	uart_puts("Free mem:  "); uart_puthex(stats.freemem); uart_putline();
	uart_puts("Used mem:  "); uart_puthex(stats.usedmem); uart_putline();

	//elf_parse_header((void*)&_binary_kernel_stripped_elf_start);
 
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

