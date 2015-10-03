#include "page_alloc.h"

#include "panic.h"
#include "uart.h"

typedef uint8_t refcount_t;

extern refcount_t __page_alloc_table_start;
extern uint32_t __start, __end;

namespace page_alloc {
	uint32_t num_pages;
	uint32_t allocated_pages = 0;
	refcount_t * refcount_table;

	void init(uint32_t total_memory){
		uart_puts("Initialising page allocator\r\n");
		refcount_table = &__page_alloc_table_start;
		uart_puts("refcount_table = "); uart_puthex((uint32_t)refcount_table); uart_puts("\r\n");
		
		num_pages = total_memory / PAGE_SIZE;
		uart_puts("num_pages = "); uart_puthex(num_pages); uart_puts("\r\n");
	
		uint32_t pages_used_for_table = (num_pages * sizeof(refcount_t) + PAGE_SIZE - 1) / PAGE_SIZE;
		uart_puts("pages_used_for_table = "); uart_puthex(pages_used_for_table); uart_puts("\r\n");
		
		uint32_t first_used_page = ((uintptr_t)&__start) / PAGE_SIZE;
		uint32_t first_free_page = ((uintptr_t)&__end) / PAGE_SIZE + pages_used_for_table;
	
		for (uint32_t i = 0; i < num_pages; i++){
			if (
				i == 0 || //we allocate page 0 so that nullptr can be used as an invalid return value
				((i >= first_used_page) && (i < first_free_page)))
			{
				refcount_table[i] = 1;
				allocated_pages++;
			} else {
				refcount_table[i] = 0;
			}
		}
		
		uart_puts("allocated_pages = "); uart_puthex(allocated_pages); uart_puts("\r\n");
		uart_puts("\r\n");
	}

	//may need optimisation
	uintptr_t alloc() {
		//enter critical section
		asm volatile("cpsid aif");
		
		static uint32_t next_alloc = 0;
		
		uint32_t entry = next_alloc;
		uintptr_t retval = 0;
		do {
			if (refcount_table[entry] == 0) {
				//use this page
				refcount_table[entry] = 1;
				retval = entry * PAGE_SIZE;
				allocated_pages++;
			}
			entry++;
			if (entry >= num_pages){
				entry -= num_pages;
			}
			
			if (entry == next_alloc){
				panic(PanicCodes::OutOfMemory); //give up
			}
		} while (retval == 0);
		
		next_alloc = entry;
		
		//exit critical section
		asm volatile("cpsie aif");
		
		return retval;
	}
	
	uint32_t ref_acquire(uintptr_t page){
		//enter critical section
		asm volatile("cpsid aif");
		
		uint32_t retval = 0;
		
		uint32_t page_ix = page / PAGE_SIZE;
		
		if (refcount_table[page_ix] == 0) {
			panic(PanicCodes::AddRefToUnallocatedPage);
		} else {
			retval = ++refcount_table[page_ix];
		}

		//exit critical section
		asm volatile("cpsie aif");
		
		return retval;
	}
	
	uint32_t ref_release(uintptr_t page){
		//enter critical section
		asm volatile("cpsid aif");
		
		uint32_t retval = 0;
		
		uint32_t page_ix = page / PAGE_SIZE;
		
		if (refcount_table[page_ix] == 0) {
			panic(PanicCodes::ReleaseUnallocatedPage);
		} else {
			retval = --refcount_table[page_ix];
		}
		
		if (retval == 0) {
			allocated_pages--;
		}

		//exit critical section
		asm volatile("cpsie aif");
		
		return retval;
	}
}

