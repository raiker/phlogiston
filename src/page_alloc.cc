#include "page_alloc.h"

#include "utility.h"
#include "panic.h"
#include "uart.h"

PageAlloc::PageAlloc(uint32_t total_memory, refcount_t * table_location){
	uart_puts("Initialising page allocator\r\n");
	refcount_table = table_location;
	uart_puts("refcount_table = "); uart_puthex((uint32_t)refcount_table); uart_puts("\r\n");
	
	num_pages = total_memory / PAGE_SIZE;
	uart_puts("num_pages = "); uart_puthex(num_pages); uart_puts("\r\n");

	uint32_t pages_used_for_table = (num_pages * sizeof(refcount_t) + PAGE_SIZE - 1) / PAGE_SIZE;
	uart_puts("pages_used_for_table = "); uart_puthex(pages_used_for_table); uart_puts("\r\n");
	
	uint32_t first_free_page = ((uintptr_t)table_location) / PAGE_SIZE + pages_used_for_table;

	for (uint32_t i = 0; i < num_pages; i++){
		if (i < first_free_page){
			refcount_table[i] = 1;
			allocated_pages++;
		} else {
			refcount_table[i] = 0;
		}
	}
	
	uart_puts("allocated_pages = "); uart_puthex(allocated_pages); uart_puts("\r\n");
	uart_puts("\r\n");
}

MemStats PageAlloc::get_mem_stats() {
	auto lock = spinlock_cs.acquire();
	
	MemStats retval;
	
	retval.totalmem = num_pages * PAGE_SIZE;
	retval.usedmem = allocated_pages * PAGE_SIZE;
	retval.freemem = retval.totalmem - retval.usedmem;
	
	return retval;
}

//may need optimisation
uintptr_t PageAlloc::alloc(uint32_t size) {
	//supports size = {1,4,256}
	//pages are 1 page (4KiB) of memory, aligned to 4KiB
	//pagetables are 4 pages (16KiB) of memory, aligned to 16KiB
	//sections are 256 pages (1MiB) of memory, aligned to 1MiB
	//supersections are 4096 pages (16MiB) of memory, aligned to 16MiB
	
	if (size != 1 && size != 4 && size != 256 && size != 4096) {
		panic(PanicCodes::IncompatibleParameter);
	}
	
	//enter critical section
	auto lock = spinlock_cs.acquire();
	
	uint32_t &next_alloc = (size == 1) ? next_alloc_1 : (size == 4) ? next_alloc_4 : (size == 256) ? next_alloc_256 : next_alloc_4096;
	
	uint32_t entry = next_alloc;
	uintptr_t retval = 0;
	do {
		bool all_refcounts_zero = true;
		
		for (uint32_t i = 0; i < size; i++) {
			if (refcount_table[entry+i] != 0) {
				all_refcounts_zero = false;
			}
		}
		
		if (all_refcounts_zero) {
			//use this section
			for (uint32_t i = 0; i < size; i++) {
				refcount_table[entry+i] = 1;
#ifdef VERBOSE					
				uart_puts("page_alloc: Acquire ");
				uart_puthex((entry+i)*PAGE_SIZE);
				uart_putline();
#endif
			}
			retval = entry * PAGE_SIZE;
			allocated_pages += size;
		}
		entry += size;
		
		if (entry >= num_pages){
			entry -= num_pages;
		}
		
		if (entry == next_alloc){
			panic(PanicCodes::OutOfMemory); //give up
		}
	} while (retval == 0);
	
	next_alloc = entry;
	
	//exit critical section
	//spinlock_flag.clear(std::memory_order_release);
	
	//clear page to 0xcc for security/debugging
	memset(retval, 0xcc, PAGE_SIZE);
	
	return retval;
}

uint32_t PageAlloc::ref_acquire(uintptr_t page){
	//enter critical section
	auto lock = spinlock_cs.acquire();
	
	uint32_t retval = 0;
	
	uint32_t page_ix = page / PAGE_SIZE;
	
	if (page_ix > num_pages) return 0; //no-op (good for mmio etc)
	
	if (refcount_table[page_ix] == 0) {
		panic(PanicCodes::AddRefToUnallocatedPage);
	} else {
		retval = ++refcount_table[page_ix];
	}
	
#ifdef VERBOSE					
	uart_puts("page_alloc: Acquire ");
	uart_puthex(page);
	uart_putline();
#endif

	//exit critical section
	//spinlock_flag.clear(std::memory_order_release);
	
	return retval;
}

uint32_t PageAlloc::ref_release(uintptr_t page){
	//enter critical section
	auto lock = spinlock_cs.acquire();
	
	uint32_t retval = 0;
	
	uint32_t page_ix = page / PAGE_SIZE;
	
	if (page_ix > num_pages) return 0; //no-op (good for mmio etc)
	
	if (refcount_table[page_ix] == 0) {
		panic(PanicCodes::ReleaseUnallocatedPage);
	} else {
		retval = --refcount_table[page_ix];
	}
	
	if (retval == 0) {
		allocated_pages--;
	}

#ifdef VERBOSE					
	uart_puts("page_alloc: Release ");
	uart_puthex(page);
	uart_putline();
#endif

	//exit critical section
	//spinlock_flag.clear(std::memory_order_release);
	
	return retval;
}

void PageAlloc::ref_acquire(uintptr_t page, uint32_t size){
	for (uint32_t i = 0; i < size; i++){
		ref_acquire(page + i * PAGE_SIZE);
	}
}

void PageAlloc::ref_release(uintptr_t page, uint32_t size){
	for (uint32_t i = 0; i < size; i++){
		ref_release(page + i * PAGE_SIZE);
	}
}

//release the pages automatically reserved to accommodate the loader
//must be called EXACTLY ONCE, after the kernel is loaded
void PageAlloc::release_loader(){
	uint32_t pages_to_release = ((uintptr_t)refcount_table) / PAGE_SIZE;
	
	ref_release(0, pages_to_release);
}

