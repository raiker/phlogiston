#include "pagetable.h"
#include "page_alloc.h"

const uint32_t NUM_ENTRIES = 4096;

PrePagingPageTable() {
	first_level_table = (*uint32_t)page_alloc::alloc(4); //4 pages for a first-level translation table
	
	for (uint32_t i = 0; i < NUM_ENTRIES; i++){
		first_level_table[i] = 0x00000000;
	}
	
	/*second_level_table_addrs = (**SecondLevelTableAddr)page_alloc::alloc(1); //1 page for the addresses of the second level tables
	
	for (uint32_t i = 0; i < MAX_SECOND_LEVEL_TABLES; i++){
		second_level_table_addrs[i] = {0x00000000, nullptr);
	}*/
}

PrePagingPageTable::~PageTable() {
	/*//free any second level tables that might have been allocated
	for (uint32_t i = 0; i < MAX_SECOND_LEVEL_TABLES; i++){
		if (second_level_table_addrs[i].physical_addr != 0x00000000){
			page_alloc::ref_release(second_level_table_addrs[i].physical_addr);
		}
	}
	
	//free the second level table addresses page
	//page_alloc::ref_release(second_level_table_addrs); //need physical address...*/
	
	uintptr_t page_base = (uintptr_t)first_level_table;
	for (uint32_t i = 0; i < 4; i++){
		page_alloc::ref_release(page_base + i * 0x1000); //decrement the reference counts on the four pages
	}
}

Result<uintptr_t> PrePagingPageTable::virtual_to_physical(uintptr_t virtual_address) {
	//see if the address is mapped
	uint32_t first_level_index = virtual_address >> 20;
	uint32_t & first_level_entry = first_level_table[first_level_index];
	
	switch (first_level_entry & 0x3) {
		case 1:
			//second-level table
			{
				uint32_t * second_level_table = (uint32 *)(first_level_entry & 0xfffffc00);
				uint32_t second_level_index = (virtual_address >> 12) & 0xff;
				
				uint32_t & second_level_entry = second_level_table[second_level_index];
				
				if (second_level_entry & 0x2) {
					//page is committed
					uintptr_t address = (second_level_entry & 0xfffff000) | (virtual_address & 0x00000fff);
					return Result<uintptr_t>::success(address);
				} else {
					//page is unallocated or reserved (we don't use large pages)
					return Result<uintptr_t>::failure();
				}
			}
			break;
		case 2:
			//section or supersection
			if (first_level_entry & (1<<18)){
				//supersection
				uintptr_t address = (first_level_entry & 0xff000000) | (virtual_address & 0x00ffffff);
				return Result<uintptr_t>::success(address);
			} else {
				//regular section
				uintptr_t address = (first_level_entry & 0xfff00000) | (virtual_address & 0x000fffff);
				return Result<uintptr_t>::success(address);
			}
			break;
		default:
			//memory is not currently committed
			return Result<uintptr_t>::failure();
	}
}

Result<uintptr_t> PrePagingPageTable::physical_to_virtual(uintptr_t physical_address) {
	//very slow, avoid if possible
}

