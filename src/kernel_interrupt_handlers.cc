#include "uart.h"
#include "mmio.h"
#include "sys_timer.h"

extern "C"
uint32_t svc_entry(uint32_t a, uint32_t b, uint32_t c, uint32_t d){
	uart_puts("System call from higher-half!\r\n");
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
void irq_handler(uint32_t *saved_regs){
	uart_puts("IRQ triggered\r\n");
	
	//saved_regs[13] = 0xdeadbeef;
	
	uint32_t IRQ_BASIC_PENDING = MMIO_BASE + 0xb000 + 0x200;
	uint32_t IRQ_PENDING_1 = MMIO_BASE + 0xb000 + 0x204;
	//uint32_t basic_pending = mmio_read(IRQ_BASIC_PENDING);
	uint32_t pending_1 = mmio_read(IRQ_PENDING_1);
	
	if (pending_1 & 0x1){
		//timer interrupt
		timer::update_comparison_value();
	}
}

extern "C"
void fiq_handler(){
	uart_puts("FIQ triggered\r\n");
}

