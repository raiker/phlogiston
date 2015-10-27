#include "pagetable.h"
#include "page_alloc.h"
#include "panic.h"

//IMPLEMENTATION INFO
//bit 2 in a "Translation fault" descriptor (i.e. bits [1:0] == 2'b00) represents:
// 0 = page is not reserved
// 1 = page is reserved

PrePagingPageTable::PrePagingPageTable(bool is_supervisor) {
	if (is_supervisor){
		//supervisor (4k entries)
		first_level_table = (uint32_t*)page_alloc::alloc(4); //4 pages
		first_level_num_entries = FIRST_LEVEL_SUPERVISOR_ENTRIES;
	} else {
		//user (2k entries)
		first_level_table = (uint32_t*)page_alloc::alloc(2); //2 pages
		first_level_num_entries = FIRST_LEVEL_USER_ENTRIES;
	}
	
	for (uint32_t i = 0; i < first_level_num_entries; i++){
		first_level_table[i] = 0x00000000;
	}
	
	//allocate the page at 0x00000000 to catch null dereferences
	uint32_t * second_level_table = create_second_level_table();
	first_level_table[0x000] = (uint32_t)second_level_table | 0x1 | (SUPERVISOR_DOMAIN << 5);
	
	second_level_table[0x00] = 0x00000000 | 0x3;
	
	/*second_level_table_addrs = (**SecondLevelTableAddr)page_alloc::alloc(1); //1 page for the addresses of the second level tables
	
	for (uint32_t i = 0; i < MAX_SECOND_LEVEL_TABLES; i++){
		second_level_table_addrs[i] = {0x00000000, nullptr);
	}*/
}

uint32_t * PrePagingPageTable::create_second_level_table() {
	uint32_t * second_level_table = (uint32_t*)page_alloc::alloc(1); //4 times as much space as we need...
	
	for (uint32_t i = 0; i < SECOND_LEVEL_ENTRIES; i++) {
		second_level_table[i] = 0x00000000;
	}
	
	return second_level_table;
}

PrePagingPageTable::~PrePagingPageTable() {
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

Result<uintptr_t> PrePagingPageTable::allocate(size_t bytes, AllocationGranularity granularity) {
	/*static uintptr_t next_alloc_page = 0x00000000;
	static uintptr_t next_alloc_section = 0x00000000;
	static uintptr_t next_alloc_supersection = 0x00000000;*/
	
	if (granularity != AllocationGranularity::Page &&
		granularity != AllocationGranularity::Section &&
		granularity != AllocationGranularity::Supersection)
	{
		panic(PanicCodes::IncompatibleParameter);
	}
	
	auto lock = spinlock_cs.acquire();
	
	//TODO: use bytes
	
	//slow, but packs fairly efficiently
	if (granularity == AllocationGranularity::Page){
		//trawl through all the second-level page tables looking for a space
		for (uint32_t i = 0; i < first_level_num_entries; i++){
			uint32_t & first_level_entry = first_level_table[i];
			if ((first_level_entry & 0x3) == 1){
				//it's a second-level table
				//see if there are any empty slots
				
				uint32_t * second_level_table = (uint32_t *)(first_level_entry & 0xfffffc00);
				for (uint32_t j = 0; j < SECOND_LEVEL_ENTRIES; j++){
					uint32_t & second_level_entry = second_level_table[j];
					
					if ((second_level_entry & 0x7) == 0x0){
						//this page is usable
						
						//TODO: permissions bits
						uint32_t base_addr = page_alloc::alloc(1);
						second_level_entry = base_addr | 0x00000002;
						
						return Result<uintptr_t>::success(i * SECTION_SIZE + j * PAGE_SIZE);
					}
				}
			}
		}
		
		//if we get here, there were no free pages
		//create a new second-level table
		for (uint32_t i = 0; i < first_level_num_entries; i++){
			uint32_t & first_level_entry = first_level_table[i];
			if ((first_level_entry & 0x7) == 0x0){
				//it's an unreserved free section
				
				//TODO: permissions bits
				uint32_t * second_level_table = create_second_level_table();
				first_level_table[i] = (uint32_t)second_level_table | 0x1 | (SUPERVISOR_DOMAIN << 5);
				
				uint32_t base_addr = page_alloc::alloc(1);
				second_level_table[0x00] = base_addr | 0x00000002;
				
				return Result<uintptr_t>::success(i * SECTION_SIZE);
			}
		}
		
		//if we get here, we have no address space left (i.e. it's all been reserved (or allocated... ^_^))
		return Result<uintptr_t>::failure();
	} else if (granularity == AllocationGranularity::Section) {
		//Section (1MiB)
		for (uint32_t i = 0; i < first_level_num_entries; i++){
			uint32_t & first_level_entry = first_level_table[i];
			if ((first_level_entry & 0x7) == 0x0){
				//it's an unreserved free section
				
				//TODO: permissions bits
				uint32_t base_addr = page_alloc::alloc(4);
				first_level_entry = base_addr | 0x00000002;
				
				return Result<uintptr_t>::success(i * SECTION_SIZE);
			}
		}
		
		//if we get here, we have no address space left (i.e. it's all been reserved (or allocated... ^_^))
		return Result<uintptr_t>::failure();
	} else {
		//Supersection (16 consecutive sections)
		for (uint32_t i = 0; i < first_level_num_entries; i += 16){
			bool all_sections_free = true;
			for (uint32_t j = 0; j < 16; j++){
				uint32_t & first_level_entry = first_level_table[i+j];
				all_sections_free &= ((first_level_entry & 0x7) == 0x0);
			}
			if (all_sections_free){
				//we have 16 consecutive free sections
				
				//TODO: permissions bits
				uint32_t base_addr = page_alloc::alloc(16);
				uint32_t new_entry = base_addr | 0x00040002;
				
				for (uint32_t j = 0; j < 16; j++){
					first_level_table[i+j] = new_entry;
				}
				
				return Result<uintptr_t>::success(i * SECTION_SIZE);
			}
		}
		
		//if we get here, we have no address space left (i.e. it's all been reserved (or allocated... ^_^))
		return Result<uintptr_t>::failure();
	}
}

Result<uintptr_t> PrePagingPageTable::virtual_to_physical_internal(uintptr_t virtual_address) {
	//see if the address is mapped
	uint32_t first_level_index = virtual_address >> 20;
	
	if (first_level_index >= first_level_num_entries) {
		return Result<uintptr_t>::failure();
	}
	
	uint32_t & first_level_entry = first_level_table[first_level_index];
	
	switch (first_level_entry & 0x3) {
		case 1:
			//second-level table
			{
				uint32_t * second_level_table = (uint32_t *)(first_level_entry & 0xfffffc00);
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

Result<uintptr_t> PrePagingPageTable::physical_to_virtual_internal(uintptr_t physical_address) {
	//very slow, avoid if possible
	//requires iterating through all the first- and second-level page table entries
	//returns only the first virtual mapping that matches the target
	for (uint32_t i = 0; i < first_level_num_entries; i++){
		uint32_t & first_level_entry = first_level_table[i];
		
		switch (first_level_entry & 0x3) {
			case 1:
				//second-level table
				{
					uint32_t * second_level_table = (uint32_t *)(first_level_entry & 0xfffffc00);
					
					for (uint32_t j = 0; j < SECOND_LEVEL_ENTRIES; j++) {
						uint32_t & second_level_entry = second_level_table[j];
						
						if (second_level_entry & 0x2) {
							//page is committed
							if ((second_level_entry & 0xfffff000) == (physical_address & 0xfffff000)){
								//match
								uintptr_t address = (first_level_entry & 0xfffff000) | (physical_address & 0x00000fff);
								return Result<uintptr_t>::success(address);
							}
						}
					}
				}
				break;
			case 2:
				//section or supersection
				if (first_level_entry & (1<<18)){
					//supersection
					if ((first_level_entry & 0xff000000) == (physical_address & 0xff000000)){
						//match
						uintptr_t address = (first_level_entry & 0xff000000) | (physical_address & 0x00ffffff);
						return Result<uintptr_t>::success(address);
					}
				} else {
					//regular section
					if ((first_level_entry & 0xfff00000) == (physical_address & 0xfff00000)){
						//match
						uintptr_t address = (first_level_entry & 0xfff00000) | (physical_address & 0x000fffff);
						return Result<uintptr_t>::success(address);
					}
				}
				break;
			default:
				//memory is not currently committed
				;
		}
	}
	
	return Result<uintptr_t>::failure();
}

Result<uintptr_t> PrePagingPageTable::virtual_to_physical(uintptr_t virtual_address) {
	auto lock = spinlock_cs.acquire();
	
	return virtual_to_physical_internal(virtual_address);
}

Result<uintptr_t> PrePagingPageTable::physical_to_virtual(uintptr_t physical_address) {
	auto lock = spinlock_cs.acquire();
	
	return physical_to_virtual_internal(physical_address);
}

