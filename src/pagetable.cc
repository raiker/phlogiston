#include "pagetable.h"
#include "page_alloc.h"
#include "panic.h"
#include "uart.h"

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
	for (uint32_t i = 0; i < first_level_num_entries / (PAGE_SIZE / sizeof(uint32_t)); i++){
		page_alloc::ref_release(page_base + i * 0x1000); //decrement the reference counts on the four pages
	}
}

Result<uintptr_t> PageTableBase::allocate(size_t bytes, AllocationGranularity granularity) {
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
	
	if (granularity == AllocationGranularity::Page && bytes >= (SECTION_SIZE / 2)){
		granularity = AllocationGranularity::Section;
	}
	
	if (bytes == 0){
		return Result<uintptr_t>::failure();
	}
	
	uint32_t * first_level_table = get_first_level_table_address();
	
	//slow, but packs fairly efficiently
	if (granularity == AllocationGranularity::Page){
		uint32_t num_pages = bytes / PAGE_SIZE + (bytes & (PAGE_SIZE-1) ? 1 : 0);
		
		//numpages is guaranteed to be <= 128
		
		//trawl through all the second-level page tables looking for a space
		for (uint32_t i = 0; i < first_level_num_entries; i++){
			uint32_t contiguous_free_pages = 0;
			
			uint32_t & first_level_entry = first_level_table[i];
			if ((first_level_entry & 0x3) == 1){
				//it's a second-level table
				//see if there are any empty slots
				
				uint32_t * second_level_table = (uint32_t *)(first_level_entry & 0xfffffc00);
				for (uint32_t j = 0; j < SECOND_LEVEL_ENTRIES; j++){
					uint32_t & second_level_entry = second_level_table[j];
					
					if ((second_level_entry & 0x7) == 0x0){
						//this page is usable
						contiguous_free_pages++;
					} else {
						contiguous_free_pages = 0;
					}
					
					if (contiguous_free_pages == num_pages){
						//we have enough contiguous pages in this second-level table to complete the allocation
						uint32_t start_index = j - num_pages + 1;
						for (uint32_t k = start_index; k < start_index + num_pages; k++) {
							//TODO: permissions bits
							uint32_t base_addr = page_alloc::alloc(1);
							second_level_table[k] = base_addr | 0x00000002;
						}
						
						return Result<uintptr_t>::success(i * SECTION_SIZE + start_index * PAGE_SIZE);
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
				
				for (uint32_t j = 0; j < num_pages; j++){
					uint32_t base_addr = page_alloc::alloc(1);
					second_level_table[j] = base_addr | 0x00000002;
				}
				
				return Result<uintptr_t>::success(i * SECTION_SIZE);
			}
		}
		
		//if we get here, we have insufficient address space left (i.e. it's all been reserved (or allocated... ^_^))
		return Result<uintptr_t>::failure();
	} else if (granularity == AllocationGranularity::Section) {
		//Section (1MiB)
		uint32_t num_sections = bytes / SECTION_SIZE + (bytes & (SECTION_SIZE-1) ? 1 : 0);
		uint32_t contiguous_free_sections = 0;
		
		for (uint32_t i = 0; i < first_level_num_entries; i++){
			uint32_t & first_level_entry = first_level_table[i];
			if ((first_level_entry & 0x7) == 0x0){
				//it's an unreserved free section
				contiguous_free_sections++;
			} else {
				contiguous_free_sections = 0;
			}
			
			if (contiguous_free_sections == num_sections){
				uint32_t start_index = i - num_sections + 1;
				
				for (uint32_t j = start_index; j < start_index + num_sections; j++){
					//TODO: permissions bits
					uint32_t base_addr = page_alloc::alloc(256);
					first_level_table[j] = base_addr | 0x00000002;
				}
				
				return Result<uintptr_t>::success(start_index * SECTION_SIZE);
			}
		}
		
		//if we get here, we have insufficient address space left (i.e. it's all been reserved (or allocated... ^_^))
		return Result<uintptr_t>::failure();
	} else {
		//Supersection (16 consecutive sections)
		uint32_t num_supersections = bytes / SUPERSECTION_SIZE + (bytes & (SUPERSECTION_SIZE-1) ? 1 : 0);
		uint32_t contiguous_free_supersections = 0;
		
		for (uint32_t i = 0; i < first_level_num_entries; i += 16){
			bool all_sections_free = true;
			for (uint32_t j = 0; j < 16; j++){
				uint32_t & first_level_entry = first_level_table[i+j];
				all_sections_free &= ((first_level_entry & 0x7) == 0x0);
			}
			if (all_sections_free){
				//we have 16 consecutive free sections
				contiguous_free_supersections++;
			} else {
				contiguous_free_supersections = 0;
			}
			
			if (contiguous_free_supersections == num_supersections){
				uint32_t start_index = i - 16 * (num_supersections - 1);
				for (uint32_t k = start_index; k < start_index + (num_supersections * 16); k += 16){
					//TODO: permissions bits
					uint32_t base_addr = page_alloc::alloc(4096);
					uint32_t new_entry = base_addr | 0x00040002;
					
					for (uint32_t j = 0; j < 16; j++){
						first_level_table[k+j] = new_entry;
					}
				}
				
				return Result<uintptr_t>::success(start_index * SECTION_SIZE);
			}
		}
		
		//if we get here, we have no address space left (i.e. it's all been reserved (or allocated... ^_^))
		return Result<uintptr_t>::failure();
	}
}

Result<uintptr_t> PageTableBase::virtual_to_physical_internal(uintptr_t virtual_address) {
	//see if the address is mapped
	uint32_t first_level_index = virtual_address >> 20;
	
	if (first_level_index >= first_level_num_entries) {
		return Result<uintptr_t>::failure();
	}
	
	uint32_t & first_level_entry = get_first_level_table_address()[first_level_index];
	
	switch (first_level_entry & 0x3) {
		case 1:
			//second-level table
			{
				uint32_t * second_level_table = get_second_level_table_address(first_level_entry & 0xfffffc00);
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

Result<uintptr_t> PageTableBase::physical_to_virtual_internal(uintptr_t physical_address) {
	//very slow, avoid if possible
	//requires iterating through all the first- and second-level page table entries
	//returns only the first virtual mapping that matches the target
	uint32_t * first_level_table = get_first_level_table_address();
	
	for (uint32_t i = 0; i < first_level_num_entries; i++){
		uint32_t & first_level_entry = first_level_table[i];
		
		switch (first_level_entry & 0x3) {
			case 1:
				//second-level table
				{
					uint32_t * second_level_table = get_second_level_table_address(first_level_entry & 0xfffffc00);
					
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

uint32_t * PrePagingPageTable::get_first_level_table_address() {
	return first_level_table;
}

uint32_t * PrePagingPageTable::get_second_level_table_address(uintptr_t physical_base_address){
	return (uint32_t*)physical_base_address;
}

Result<uintptr_t> PageTableBase::virtual_to_physical(uintptr_t virtual_address) {
	auto lock = spinlock_cs.acquire();
	
	return virtual_to_physical_internal(virtual_address);
}

Result<uintptr_t> PageTableBase::physical_to_virtual(uintptr_t physical_address) {
	auto lock = spinlock_cs.acquire();
	
	return physical_to_virtual_internal(physical_address);
}

void PageTableBase::print_table_info() {
	auto lock = spinlock_cs.acquire();
	
	uintptr_t unmapped_start;
	uint32_t unmapped_count = 0;
	
	for (uint32_t i = 0; i < first_level_num_entries; i++){
		uint32_t &first_level_entry = get_first_level_table_address()[i];
		uintptr_t section_base = i * SECTION_SIZE;
		
		uint32_t mapping_type = first_level_entry & 0x3;
		if (mapping_type == 0){
			//unmapped
			if (unmapped_count++ == 0){
				unmapped_start = section_base;
			}
		} else {
			if (unmapped_count > 0){
				uart_puthex(unmapped_start);
				uart_puts("\t");
				uart_putdec(unmapped_count);
				uart_puts(" unmapped sections\r\n");
				unmapped_count = 0;
			}
			
			if (mapping_type == 1){
				uart_puthex(section_base);
				uart_puts("\tsecond-level page table\r\n");
				
				print_second_level_table_info(get_second_level_table_address(first_level_entry & 0xfffffc00), section_base);
			} else if (mapping_type == 2){
				uart_puthex(section_base);

				uintptr_t address;
				if (first_level_entry & (1<<18)){
					//uart_puthex(first_level_entry);
					//supersection
					address = (first_level_entry & 0xff000000);
					uart_puts("\tsupersection mapped to ");
				} else {
					//regular section
					address = (first_level_entry & 0xfff00000);
					uart_puts("\tsection mapped to ");
				}
				
				uart_puthex(address);
				uart_puts("\r\n");
			} else {
				uart_puthex(section_base);
				uart_puts("\tinvalid descriptor!\r\n");
			}
		}
	}
	
	if (unmapped_count > 0){
		uart_puthex(unmapped_start);
		uart_puts("\t");
		uart_putdec(unmapped_count);
		uart_puts(" unmapped sections\r\n");
		unmapped_count = 0;
	}
}

void PageTableBase::print_second_level_table_info(uint32_t * table, uintptr_t base) {
	uintptr_t unmapped_start;
	uint32_t unmapped_count = 0;
	
	for (uint32_t i = 0; i < SECOND_LEVEL_ENTRIES; i++){
		uint32_t &second_level_entry = table[i];
		uintptr_t page_base = base + i * PAGE_SIZE;
		
		if (!(second_level_entry & 0x2)){
			//unmapped
			if (unmapped_count++ == 0){
				unmapped_start = page_base;
			}
		} else {
			if (unmapped_count > 0){
				uart_puthex(unmapped_start);
				uart_puts("\t");
				uart_putdec(unmapped_count);
				uart_puts(" unmapped pages\r\n");
				unmapped_count = 0;
			}
			
			uintptr_t address = (second_level_entry & 0xfffff000);
			
			uart_puts("\t");
			uart_puthex(page_base);
			uart_puts("\tpage mapped to ");
			uart_puthex(address);
			uart_puts("\r\n");
		}
	}
	
	if (unmapped_count > 0){
		uart_puts("\t");
		uart_puthex(unmapped_start);
		uart_puts("\t");
		uart_putdec(unmapped_count);
		uart_puts(" unmapped pages\r\n");
		unmapped_count = 0;
	}
}

