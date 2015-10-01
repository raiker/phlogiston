#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "uart.h"
#include "atags.h"
  
extern "C" /* Use C linkage for kernel_main. */
void kernel_main(uint32_t r0, uint32_t r1, void * atags, uint32_t cpsr_saved)
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
	
	debug_atags(atags);
	
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
	
	asm volatile(
		"ldr r0, =ivt_start\n"
		"mcr p15, 0, r0, c12, c0, 0"
		: : : "r0");
	
	asm volatile("svc #0");
 
	while ( true )
		uart_putc(uart_getc());
}

extern "C"
uint32_t svc_entry(uint32_t a, uint32_t b, uint32_t c, uint32_t d){
	uart_puts("System call!\r\n");
	return 0;
}
