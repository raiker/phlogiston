#include "pagetable.h"
#include "page_alloc.h"

const uint32_t NUM_ENTRIES = 4096;

PageTable() {
	first_level_pa = (**uint32_t)page_alloc::alloc(4); //4 pages for a first-level translation table
	
	for (uint32_t i = 0; i < NUM_ENTRIES; i++){
		first_level_pa[i] = 0x00000000;
	}
}

PageTable::~PageTable() {
	uintptr_t page_base = first_level_pa;
	for (uint32_t i = 0; i < 4; i++){
		page_alloc::ref_release(page_base + i * 0x1000); //decrement the reference counts on the four pages
	}
}

