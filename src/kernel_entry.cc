#include "pagetable.h"
#include "panic.h"
#include "uart.h"

extern "C"
void kernel_entry(PageTable *identity_overlay, PageTable *supervisor_pagetable) {
	uart_puts("Running from higher-half\r\n");
	
	//supervisor_pagetable->print_table_info();
	
	panic(PanicCodes::AssertionFailure);
}
