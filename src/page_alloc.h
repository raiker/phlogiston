#pragma once

#include "common.h"
#include "spinlock.h"

struct MemStats {
	uint32_t totalmem;
	uint32_t freemem;
	uint32_t usedmem;
};

typedef uint8_t refcount_t;

class PageAlloc {
private:
	refcount_t * refcount_table;
	uint32_t num_pages;
	uint32_t allocated_pages = 0;
	Spinlock spinlock_cs;
	
	uint32_t next_alloc_1 = 0; //page
	uint32_t next_alloc_4 = 0; //pagetable
	uint32_t next_alloc_256 = 0; //section
	uint32_t next_alloc_4096 = 0; //supersection
	
public:
	PageAlloc(uint32_t total_memory, refcount_t * table_location); //table_location is also the end of used memory
	uintptr_t alloc(uint32_t size);
	uint32_t ref_acquire(uintptr_t page);
	uint32_t ref_release(uintptr_t page);
	void ref_acquire(uintptr_t page, uint32_t size);
	void ref_release(uintptr_t page, uint32_t size);
	MemStats get_mem_stats();
};

