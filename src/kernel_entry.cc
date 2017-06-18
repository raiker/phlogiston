#include "pagetable.h"
#include "panic.h"
#include "uart.h"
#include "process.h"
#include "sys_timer.h"
#include "mmio.h"

//global pointers
PageTable *g_supervisor_pagetable, *g_identity_overlay;
PageAlloc *g_page_alloc;

void kernel_init() __attribute__((noreturn));

extern "C"
void kernel_entry(PageTable *identity_overlay, PageTable *supervisor_pagetable, PageAlloc * page_alloc) {
	uart_puts("Running from higher-half\r\n");
	
	//move to new interrupt handlers
	asm volatile(
		"ldr r0, =ivt_start\n"
		"mcr p15, 0, r0, c12, c0, 0"
		: : : "r0");
	
	asm volatile("svc #0");
	
	//free the pages reserved for the loader
	page_alloc->release_loader();
	
	supervisor_pagetable->print_table_info();
	
	//copy the resources from the loader into the kernel's address space
	size_t global_block_size = sizeof(PageTable) * 2 + sizeof(PageAlloc);
	uint32_t pages_to_alloc = get_num_allocation_units(global_block_size, AllocationGranularity::Page);
	
	auto allocation = supervisor_pagetable->reserve_allocate(pages_to_alloc, AllocationGranularity::Page);
	
	if (allocation.is_error()){
		panic(PanicCodes::AssertionFailure);
	}
	
	g_supervisor_pagetable = (PageTable*)allocation.get_value();
	g_identity_overlay = (PageTable*)(allocation.get_value() + sizeof(PageTable));
	g_page_alloc = (PageAlloc*)(allocation.get_value() + 2 * sizeof(PageTable));
	
	memcpy((uintptr_t)g_supervisor_pagetable, (uintptr_t)supervisor_pagetable, sizeof(PageTable));
	memcpy((uintptr_t)g_identity_overlay, (uintptr_t)identity_overlay, sizeof(PageTable));
	memcpy((uintptr_t)g_page_alloc, (uintptr_t)page_alloc, sizeof(PageAlloc));
	
	kernel_init();
	
//	Process p(*page_alloc, *supervisor_pagetable);
//	
//	p.create_thread(0x00010000);
//	
//	panic(PanicCodes::AssertionFailure);
}

void kernel_init(){
	//enable timer interrupts
	timer::init_timer_interrupt();
	
	panic(PanicCodes::AssertionFailure);
}
