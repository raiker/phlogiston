#include "panic.h"

#include "uart.h"

void panic(PanicCodes code){
	uart_puts("Kernel Panic\r\n");
	uart_puthex((uint32_t)code);
	const char * msg;
	
	switch(code){
		case PanicCodes::OutOfMemory:
			msg = "Out of memory";
			break;
		case PanicCodes::InvalidParameter:
			msg = "Invalid parameter";
			break;
		case PanicCodes::IncompatibleParameter:
			msg = "Incompatible parameter";
			break;
		case PanicCodes::AddRefToUnallocatedPage:
			msg = "Attempted to add a reference to an unallocated page";
			break;
		case PanicCodes::ReleaseUnallocatedPage:
			msg = "Attempted to release an unallocated page";
			break;
		case PanicCodes::TooManyReferences:
			msg = "Attempted to exceed maximum reference count";
			break;
		case PanicCodes::NoMemory:
			msg = "No memory ATAG found";
			break;
		case PanicCodes::NonZeroBase:
			msg = "System RAM not based at address 0";
			break;
		case PanicCodes::AssertionFailure:
			msg = "Assertion failure";
			break;
		default:
			msg = "Unknown error";
			break;
	}
	
	uart_puts(msg);
	uart_puts("\r\n");
	
	while (true){
		asm volatile("wfe");
	}
}

