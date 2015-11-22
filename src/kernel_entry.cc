#include "pagetable.h"
#include "panic.h"
#include "uart.h"
#include "process.h"

extern "C"
void kernel_entry(PageTable *identity_overlay, PageTable *supervisor_pagetable, PageAlloc * page_alloc) {
	uart_puts("Running from higher-half\r\n");
	
	supervisor_pagetable->print_table_info();
	
	Process p(*page_alloc, *supervisor_pagetable);
	
	panic(PanicCodes::AssertionFailure);
	
	p.create_thread(0x00010000);
	
	panic(PanicCodes::AssertionFailure);
}
