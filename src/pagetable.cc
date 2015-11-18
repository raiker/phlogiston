#include "pagetable.h"
#include "panic.h"
#include "uart.h"

#include <atomic>
#include <algorithm>

static uint32_t get_allocation_pages(AllocationGranularity granularity){
	switch (granularity){
		case AllocationGranularity::Page:
			return 1;
		case AllocationGranularity::Section:
			return 256;
		case AllocationGranularity::Supersection:
			return 4096;
		default:
			panic(PanicCodes::IncompatibleParameter);
	}
}

//IMPLEMENTATION INFO
//bit 2 in a "Translation fault" descriptor (i.e. bits [1:0] == 2'b00) represents:
// 0 = page is not reserved
// 1 = page is reserved

PageTable::PageTable(PageAlloc &_page_alloc, bool is_supervisor, bool is_reference_counted) :
	page_alloc(_page_alloc)
{
	first_level_table = (uint32_t*)page_alloc.alloc(4); //4 pages for both modes
	
	reference_counted = is_reference_counted;
	
	if (is_supervisor){
		//supervisor (4k entries)
		first_level_num_entries = FIRST_LEVEL_SUPERVISOR_ENTRIES;
	} else {
		//user (2k entries)
		first_level_num_entries = FIRST_LEVEL_USER_ENTRIES;
	}
	
	for (uint32_t i = 0; i < first_level_num_entries; i++){
		first_level_table[i] = 0x00000000;
	}
	
	//allocate the page at 0x00000000 to catch null dereferences
	/*auto reservation = reserve(0x00000000, 1, AllocationGranularity::Page);
	if (reservation.is_success){
		map(0x00000000, 0x00000000, 1, AllocationGranularity::Page);
		auto descriptor = get_page_descriptor(0x00000000);
		if (descriptor.is_success){
			*descriptor.value |= 0x1; //NX bit
		}
	}*/
}

uint32_t * PageTable::create_second_level_table() {
	uint32_t * second_level_table = (uint32_t*)page_alloc.alloc(1); //4 times as much space as we need...
	
	for (uint32_t i = 0; i < SECOND_LEVEL_ENTRIES; i++) {
		second_level_table[i] = 0x00000000;
	}
	
	return second_level_table;
}

PageTable::~PageTable() {
	//release every page that has been allocated
	uint32_t* first_level_table = get_first_level_table_address();
	for (uint32_t i = 0; i < first_level_num_entries; i++){
		uint32_t & first_level_entry = first_level_table[i];
		
		if ((first_level_entry & 0x3) == 0x1){
			//second-level table
			uint32_t * second_level_table = get_second_level_table_address(first_level_entry & 0xfffffc00);
			
			for (uint32_t j = 0; j < SECOND_LEVEL_ENTRIES; j++){
				uint32_t & second_level_entry = second_level_table[j];
				
				if (second_level_entry & 0x2){
					uintptr_t physical_address = second_level_entry & 0xfffff000;
					
					if (reference_counted){
						page_alloc.ref_release(physical_address);
					}
				}
			}
			
			//free table - done even if the table isn't reference-counted
			page_alloc.ref_release((uintptr_t)second_level_table);
		} else if ((first_level_entry & 0x3) == 0x2){
			uintptr_t physical_address;
			if (first_level_entry & 0x00040000) {
				//supersection
				physical_address = (first_level_entry & 0xff000000) | ((i << 20) & 0x00f00000);
			} else {
				//regular section
				physical_address = first_level_entry & 0xfff00000;
			}
			
			if (reference_counted){
				page_alloc.ref_release(physical_address, SECOND_LEVEL_ENTRIES);
			}
		}
	}
	
	uintptr_t page_base = (uintptr_t)first_level_table;
	for (uint32_t i = 0; i < 4; i++){
		page_alloc.ref_release(page_base + i * 0x1000); //decrement the reference counts on the four pages
	}
}

Result<uintptr_t> PageTable::reserve(uint32_t units, AllocationGranularity granularity){
	auto lock = spinlock_cs.acquire();
	
	switch (granularity){
		case AllocationGranularity::Page:
			return reserve_pages(units);
		case AllocationGranularity::Section:
			return reserve_sections(units);
		case AllocationGranularity::Supersection:
			return reserve_supersections(units);
		default:
			panic(PanicCodes::IncompatibleParameter);
	}
}

Result<uintptr_t> PageTable::reserve(uintptr_t address, uint32_t units, AllocationGranularity granularity){
	auto lock = spinlock_cs.acquire();
	
	switch (granularity){
		case AllocationGranularity::Page:
			return reserve_pages(address, units);
		case AllocationGranularity::Section:
			return reserve_sections(address, units);
		case AllocationGranularity::Supersection:
			return reserve_supersections(address, units);
		default:
			panic(PanicCodes::IncompatibleParameter);
	}
}

//TODO: handle partial failure
bool PageTable::allocate(uintptr_t virtual_address, uint32_t units, AllocationGranularity granularity){
#ifdef VERBOSE
	uart_puts("PageTable::allocate(virtual_address=");
	uart_puthex(virtual_address);
	uart_puts(",units=");
	uart_putdec(units);
	uart_puts(",granularity=");
	uart_putdec((uint32_t)granularity);
	uart_puts(")\r\n");
#endif
	
	if (!reference_counted){
		panic(PanicCodes::AllocationInNonReferenceCountedTable);
	}
	
	auto lock = spinlock_cs.acquire();
	
	for (uint32_t i = 0; i < units; i++){
		//allocate a block
		uintptr_t physical_address = page_alloc.alloc(get_allocation_pages(granularity));
		
		uintptr_t map_address = virtual_address + i * get_allocation_pages(granularity) * PAGE_SIZE;
		
		//map it
		bool success;
		
		switch (granularity){
			case AllocationGranularity::Page:
				success = commit_page(map_address, physical_address);
				break;
			case AllocationGranularity::Section:
				success = commit_section(map_address, physical_address);
				break;
			case AllocationGranularity::Supersection:
				success = commit_supersection(map_address, physical_address);
				break;
			default:
				panic(PanicCodes::IncompatibleParameter);
		}
		
		if (!success){
			uart_puts("failure\r\n");
		}
		
		if (!success) {
			page_alloc.ref_release(physical_address, get_allocation_pages(granularity)); //free the allocated memory
			return false;
		}
	}
	
	return true;
}

//TODO: cleanup on partial failure
bool PageTable::map(uintptr_t virtual_address, uintptr_t physical_address, uint32_t units, AllocationGranularity granularity) {
	auto lock = spinlock_cs.acquire();
	
	for (uint32_t i = 0; i < units; i++){
		uintptr_t offset = i * get_allocation_pages(granularity) * PAGE_SIZE;
		
		//map it
		bool success;
		
		switch (granularity){
			case AllocationGranularity::Page:
				success = commit_page(virtual_address + offset, physical_address + offset);
				break;
			case AllocationGranularity::Section:
				success = commit_section(virtual_address + offset, physical_address + offset);
				break;
			case AllocationGranularity::Supersection:
				success = commit_supersection(virtual_address + offset, physical_address + offset);
				break;
			default:
				panic(PanicCodes::IncompatibleParameter);
		}
		
		if (!success) return false;
	}
	
	if (reference_counted){
		//bump reference counts
		page_alloc.ref_acquire(physical_address, units * get_allocation_pages(granularity));
	}
	
	return true;
}

Result<uintptr_t> PageTable::reserve_allocate(uint32_t units, AllocationGranularity granularity){
	//doesn't acquire lock; convenience method
	
	Result<uintptr_t> reservation = reserve(units, granularity);
	if (reservation.is_success){
		if (!allocate(reservation.value, units, granularity)){
			return Result<uintptr_t>::failure();
		}
	}
	return reservation;
}

Result<uintptr_t> PageTable::reserve_allocate(uintptr_t address, uint32_t units, AllocationGranularity granularity){
	//doesn't acquire lock; convenience method
	
	Result<uintptr_t> reservation = reserve(address, units, granularity);
	if (reservation.is_success){
		if (!allocate(reservation.value, units, granularity)){
			return Result<uintptr_t>::failure();
		}
	}
	return reservation;
}

bool PageTable::commit_page(uintptr_t virtual_address, uintptr_t physical_address){
	//TODO: Permission bits
	virtual_address &= 0xfffff000;
	physical_address &= 0xfffff000;
	
	auto result = get_page_descriptor(virtual_address);
	
	if (result.is_success){
		//in order to be committed, the page needs to be reserved already
		if ((*result.value & 0x7) == 0x4){
			//reserved but not committed yet
			*result.value = physical_address | 0x00000002;
			return true;
		}
	}
	return false;
}

bool PageTable::commit_section(uintptr_t virtual_address, uintptr_t physical_address){
	//TODO: Permission bits
	virtual_address &= 0xfff00000;
	physical_address &= 0xfff00000;
	
	auto result = get_section_descriptor(virtual_address, false);
	
	if (result.is_success){
		//in order to be committed, the section needs to be reserved already
		if ((*result.value & 0x7) == 0x4){
			//reserved but not committed yet
			*result.value = physical_address | 0x00000002;
			return true;
		}
	}
	return false;
}

bool PageTable::commit_supersection(uintptr_t virtual_address, uintptr_t physical_address){
#ifdef VERBOSE
	uart_puts("PageTable::commit_supersection(virtual_address=");
	uart_puthex(virtual_address);
	uart_puts(",physical_address=");
	uart_puthex(physical_address);
	uart_puts(")\r\n");
#endif
	
	//TODO: Permission bits
	virtual_address &= 0xff000000;
	physical_address &= 0xff000000;
	
	auto result = get_section_descriptor(virtual_address, false);
	
	if (result.is_success){
		//in order to be committed, the sections need to be reserved already
		for (uint32_t i = 0; i < 16; i++){
			if ((result.value[i] & 0x07) != 0x04) return false;
		}
		for (uint32_t i = 0; i < 16; i++){
			result.value[i] = physical_address | 0x00040002;
		}
		return true;
	}
	return false;
}

Result<uint32_t*> PageTable::get_page_descriptor(uintptr_t virtual_address) {
	uint32_t first_level_index = virtual_address >> 20;
	
	if (first_level_index >= first_level_num_entries) {
		return Result<uint32_t*>::failure();
	}
	
	uint32_t & first_level_entry = get_first_level_table_address()[first_level_index];
	
	if ((first_level_entry & 0x3) == 1) {
		uint32_t * second_level_table = get_second_level_table_address(first_level_entry & 0xfffffc00);
		uint32_t second_level_index = (virtual_address >> 12) & 0xff;
	
		return Result<uint32_t*>::success(&second_level_table[second_level_index]);
	}
	
	return Result<uint32_t*>::failure();
}

Result<uint32_t*> PageTable::get_section_descriptor(uintptr_t virtual_address, bool allow_second_level) {
	uint32_t first_level_index = virtual_address >> 20;
	
	if (first_level_index >= first_level_num_entries) {
		return Result<uint32_t*>::failure();
	}
	
	uint32_t & first_level_entry = get_first_level_table_address()[first_level_index];
	
	if ((first_level_entry & 0x3) == 1 && !allow_second_level) {
		return Result<uint32_t*>::failure();
	} else {
		return Result<uint32_t*>::success(&first_level_entry);
	}
}

Result<uintptr_t> PageTable::reserve_pages(uint32_t num_pages){
	uint32_t * first_level_table = get_first_level_table_address();
	
	//trawl through all the second-level page tables looking for a space
	for (uint32_t i = 0; i < first_level_num_entries; i++){
		uint32_t contiguous_free_pages = 0;
		
		uint32_t & first_level_entry = first_level_table[i];
		if ((first_level_entry & 0x3) == 1){
			//it's a second-level table
			//see if there are any empty slots
			
			uint32_t * second_level_table = get_second_level_table_address(first_level_entry & 0xfffffc00);
			for (uint32_t j = 0; j < SECOND_LEVEL_ENTRIES; j++){
				uint32_t & second_level_entry = second_level_table[j];
				
				if ((second_level_entry & 0x7) == 0x0){
					//this page is usable
					contiguous_free_pages++;
				} else {
					contiguous_free_pages = 0;
				}
				
				if (contiguous_free_pages == num_pages){
					//we have enough contiguous pages in this second-level table to complete the reservation
					uint32_t start_index = j - num_pages + 1;
					for (uint32_t k = start_index; k < start_index + num_pages; k++) {
						second_level_table[k] = 0x00000004; //mark as reserved
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
			
			//TODO: fix this
			uint32_t * second_level_table = get_second_level_table_address((uint32_t)create_second_level_table());
			first_level_table[i] = (uint32_t)second_level_table | 0x1 | (SUPERVISOR_DOMAIN << 5);
			
			for (uint32_t j = 0; j < num_pages; j++){
				second_level_table[j] = 0x00000004;
			}
			
			return Result<uintptr_t>::success(i * SECTION_SIZE);
		}
	}
	
	//if we get here, we have insufficient address space left (i.e. it's all been reserved (or allocated... ^_^))
	return Result<uintptr_t>::failure();
}

Result<uintptr_t> PageTable::reserve_sections(uint32_t num_sections) {
	uint32_t * first_level_table = get_first_level_table_address();
	
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
				first_level_table[j] = 0x00000004;
			}
			
			return Result<uintptr_t>::success(start_index * SECTION_SIZE);
		}
	}
	
	//if we get here, we have insufficient address space left (i.e. it's all been reserved (or allocated... ^_^))
	return Result<uintptr_t>::failure();
}

Result<uintptr_t> PageTable::reserve_supersections(uint32_t num_supersections) {
	uint32_t * first_level_table = get_first_level_table_address();
	
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
				for (uint32_t j = 0; j < 16; j++){
					first_level_table[k+j] = 0x00000004;
				}
			}
			
			return Result<uintptr_t>::success(start_index * SECTION_SIZE);
		}
	}
	
	//if we get here, we have no address space left (i.e. it's all been reserved (or allocated... ^_^))
	return Result<uintptr_t>::failure();
}

Result<uintptr_t> PageTable::reserve_pages(uintptr_t base, uint32_t num_pages) {
	base &= 0xfffff000;
	
	//check the pages are all reservable
	uint32_t num_unchecked_pages = num_pages;
	uintptr_t check_base = base;
	
	while (num_unchecked_pages > 0){
		uint32_t pages_in_rest_of_section = ((check_base & 0xfff00000) - check_base + SECTION_SIZE) / PAGE_SIZE;
		uint32_t pages_to_check = std::min(num_unchecked_pages, pages_in_rest_of_section);
		
		if (!check_section_partially_reservable(check_base, pages_to_check)){
			return Result<uintptr_t>::failure();
		}
		
		num_unchecked_pages -= pages_to_check;
		check_base = (check_base & 0xfff00000) + SECTION_SIZE;
	}
	
	//if we get here, the whole thing can be allocated
	uint32_t num_unreserved_pages = num_pages;
	uintptr_t reservation_base = base;
	
	while (num_unreserved_pages > 0){
		uint32_t pages_in_rest_of_section = ((reservation_base & 0xfff00000) - reservation_base + SECTION_SIZE) / PAGE_SIZE;
		uint32_t pages_to_reserve = std::min(num_unreserved_pages, pages_in_rest_of_section);
		
		reserve_pages_from_section(reservation_base, pages_to_reserve);
		
		num_unreserved_pages -= pages_to_reserve;
		reservation_base = (reservation_base & 0xfff00000) + SECTION_SIZE;
	}
	
	return Result<uintptr_t>::success(base);
}

//checks whether num_pages pages can be reserved in the section enclosing base, starting from base
bool PageTable::check_section_partially_reservable(uintptr_t base, uint32_t num_pages) {
	auto result = get_section_descriptor(base, true);
	
	if (!result.is_success) return false;
	
	//if result is a free section, we're fine
	if ((*result.value & 0x7) == 0) return true;
	
	//second-level page table
	if ((*result.value & 0x3) == 1) {
		for (uint32_t i = 0; i < num_pages; i++){
			result = get_page_descriptor(base + i * PAGE_SIZE);
			
			if (!result.is_success || (*result.value & 0x7)) return false;
		}
		return true;
	}
	
	//section is in use
	return false;
}

//check_section_partially_reservable MUST be called beforehand; this performs no checks
void PageTable::reserve_pages_from_section(uintptr_t base, uint32_t num_pages) {
	auto result = get_section_descriptor(base, true);
	
	uint32_t * second_level_table;
	
	if ((*result.value & 0x7) == 0) {
		//create new second-level table
		//TODO: fix this
		uint32_t * new_table = create_second_level_table();
		*result.value = (uint32_t)new_table | 0x1 | (SUPERVISOR_DOMAIN << 5);
		second_level_table = get_second_level_table_address((uintptr_t)new_table);
	} else {
		second_level_table = get_second_level_table_address(*result.value & 0xfffffc00);
	}
	
	uint32_t start_index = (base & 0x000ff000) >> 12;
	
	for (uint32_t i = 0; i < num_pages; i++){
		second_level_table[start_index + i] = 0x00000004;
	}
}

Result<uintptr_t> PageTable::reserve_sections(uintptr_t base, uint32_t num_sections) {
	base &= 0xfff00000;
	
	for (uint32_t i = 0; i < num_sections; i++){
		auto result = get_section_descriptor(base + i * SECTION_SIZE, false);
		if (!result.is_success || *result.value & 0x7){
			return Result<uintptr_t>::failure();
		}
	}
	
	for (uint32_t i = 0; i < num_sections; i++){
		auto result = get_section_descriptor(base + i * SECTION_SIZE, false);
		*result.value = 0x00000004;
	}
	
	return Result<uintptr_t>::success(base);
}

Result<uintptr_t> PageTable::reserve_supersections(uintptr_t base, uint32_t num_supersections) {
	base &= 0xff000000;
	
	for (uint32_t i = 0; i < num_supersections * 16; i++){
		auto result = get_section_descriptor(base + i * SECTION_SIZE, false);
		if (!result.is_success || *result.value & 0x7){
			return Result<uintptr_t>::failure();
		}
	}
	
	for (uint32_t i = 0; i < num_supersections * 16; i++){
		auto result = get_section_descriptor(base + i * SECTION_SIZE, false);
		*result.value = 0x00000004;
	}
	
	return Result<uintptr_t>::success(base);
}

Result<uintptr_t> PageTable::virtual_to_physical_internal(uintptr_t virtual_address) {
	//apparently this can be done in hardware
	//http://blogs.bu.edu/md/2011/12/06/tagged-tlbs-and-context-switching/
	
	/*uint32_t pa;
	asm volatile ("mcr p15, 0, %[va], c7, c8, 0" : : [va] "r" (virtual_address));
	asm volatile ("mrc p15, 0, %[pa], c7, c4, 0" : [pa] "=r" (pa));
	
	if (pa & 0x00000001){
		//abort
		return Result<uintptr_t>::failure();
	} else {
		//success
		return Result<uintptr_t>::success(pa & 0xfffffc00);
	}*/
	
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

Result<uintptr_t> PageTable::physical_to_virtual_internal(uintptr_t physical_address) {
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

uint32_t * PageTable::get_first_level_table_address() {
	return first_level_table;
}

uint32_t * PageTable::get_second_level_table_address(uintptr_t physical_base_address){
	return (uint32_t*)physical_base_address;
}

UnitState get_state_from_descriptor(uint32_t descriptor){
	if ((descriptor & 0x7) == 0){
		return UnitState::Free;
	} else if ((descriptor & 0x7) == 4){
		return UnitState::Reserved;
	} else {
		return UnitState::Committed;
	}
}

Result<UnitState> PageTable::get_unit_state(uintptr_t virtual_address, AllocationGranularity granularity) {
	auto lock = spinlock_cs.acquire();
	
	switch (granularity){
		case AllocationGranularity::Page:
			{
				Result<uint32_t*> section_descriptor = get_section_descriptor(virtual_address, false);
				if (section_descriptor.is_success){
					//it's mapped as a section
					return Result<UnitState>::success(get_state_from_descriptor(*section_descriptor.value));
				}
				//it's mapped as a second-level table
				Result<uint32_t*> page_descriptor = get_page_descriptor(virtual_address);
				if (!page_descriptor.is_success){
					return Result<UnitState>::failure();
				}
				
				return Result<UnitState>::success(get_state_from_descriptor(*page_descriptor.value));
			}
		case AllocationGranularity::Section:
			{
				Result<uint32_t*> section_descriptor = get_section_descriptor(virtual_address, false);
				if (section_descriptor.is_success){
					//it's mapped as a section
					return Result<UnitState>::success(get_state_from_descriptor(*section_descriptor.value));
				} else {
					return Result<UnitState>::failure();
				}
			}
		case AllocationGranularity::Supersection:
			{
				virtual_address &= 0xff000000;
				bool all_free = true;
				bool all_reserved = true;
				bool all_committed = true;
				
				for (uint32_t i = 0; i < 16; i++){
					Result<uint32_t*> section_descriptor = get_section_descriptor(virtual_address + i * SECTION_SIZE, false);
					if (section_descriptor.is_success){
						//it's mapped as a supersection
						if ((*section_descriptor.value & 0x7) == 0){
							all_reserved = false;
							all_committed = false;
						} else if ((*section_descriptor.value & 0x7) == 4){
							all_free = false;
							all_committed = false;
						} else {
							all_free = false;
							all_reserved = false;
						}
					} else {
						return Result<UnitState>::failure();
					}
				}
				
				if (all_free){
					return Result<UnitState>::success(UnitState::Free);
				} else if (all_reserved){
					return Result<UnitState>::success(UnitState::Reserved);
				} else if (all_committed){
					return Result<UnitState>::success(UnitState::Committed);
				} else {
					return Result<UnitState>::failure();
				}
			}
		default:
			return Result<UnitState>::failure();
	}
}

Result<uintptr_t> PageTable::virtual_to_physical(uintptr_t virtual_address) {
	auto lock = spinlock_cs.acquire();
	
	return virtual_to_physical_internal(virtual_address);
}

Result<uintptr_t> PageTable::physical_to_virtual(uintptr_t physical_address) {
	auto lock = spinlock_cs.acquire();
	
	return physical_to_virtual_internal(physical_address);
}

enum class AggregationTypes {
	Unmapped,
	Reserved,
};

void section_aggregation_display(
	uintptr_t &aggregation_start,
	uint32_t &aggregation_count,
	AggregationTypes &aggregation_type)
{
	uart_puthex(aggregation_start);
	uart_puts("\t");
	uart_putdec(aggregation_count);
	if (aggregation_type == AggregationTypes::Unmapped){
		uart_puts(" unmapped sections\r\n");
	} else {
		uart_puts(" reserved sections\r\n");
	}
	aggregation_count = 0;
}

void page_aggregation_display(
	uintptr_t &aggregation_start,
	uint32_t &aggregation_count,
	AggregationTypes &aggregation_type)
{
	uart_puts("\t");
	uart_puthex(aggregation_start);
	uart_puts("\t");
	uart_putdec(aggregation_count);
	if (aggregation_type == AggregationTypes::Unmapped){
		uart_puts(" unmapped pages\r\n");
	} else {
		uart_puts(" reserved pages\r\n");
	}
	aggregation_count = 0;
}

void PageTable::print_table_info() {
	auto lock = spinlock_cs.acquire();
	
	uintptr_t aggregation_start;
	uint32_t aggregation_count = 0;
	AggregationTypes aggregation_type;
	
	for (uint32_t i = 0; i < first_level_num_entries; i++){
		uint32_t &first_level_entry = get_first_level_table_address()[i];
		uintptr_t section_base = i * SECTION_SIZE;
		
		uint32_t mapping_type = first_level_entry & 0x3;
		if (mapping_type == 0){
			if (first_level_entry & 0x4){
				//section is reserved
				if (aggregation_count > 0 && aggregation_type != AggregationTypes::Reserved){
					section_aggregation_display(aggregation_start, aggregation_count, aggregation_type);
				}
				
				if (aggregation_count++ == 0){
					aggregation_start = section_base;
					aggregation_type = AggregationTypes::Reserved;
				}
			} else {
				//unmapped
				if (aggregation_count > 0 && aggregation_type != AggregationTypes::Unmapped){
					section_aggregation_display(aggregation_start, aggregation_count, aggregation_type);
				}
				
				if (aggregation_count++ == 0){
					aggregation_start = section_base;
					aggregation_type = AggregationTypes::Unmapped;
				}
			}
		} else {
			if (aggregation_count > 0){
				section_aggregation_display(aggregation_start, aggregation_count, aggregation_type);
			}
			
			if (mapping_type == 1){
				uintptr_t second_level_physical_address = first_level_entry & 0xfffffc00;
				uart_puthex(section_base);
				uart_puts("\tsecond-level page table (");
				uart_puthex(second_level_physical_address);
				uart_puts(")\r\n");
				
				print_second_level_table_info(get_second_level_table_address(second_level_physical_address), section_base);
			} else if (mapping_type == 2){
				uart_puthex(section_base);
				
				bool nx = first_level_entry & 0x10;
				
				uintptr_t address;
				if (first_level_entry & (1<<18)){
					//uart_puthex(first_level_entry);
					//supersection
					address = (first_level_entry & 0xff000000);
					uart_puts("\tsupersection mapped to ");
					
					i += 15;
				} else {
					//regular section
					address = (first_level_entry & 0xfff00000);
					uart_puts("\tsection mapped to ");
				}
				
				uart_puthex(address);
				
				if (nx){
					uart_puts(" NX");
				}
				uart_puts("\r\n");
			} else {
				uart_puthex(section_base);
				uart_puts("\tinvalid descriptor!\r\n");
			}
		}
	}
	
	if (aggregation_count > 0){
		section_aggregation_display(aggregation_start, aggregation_count, aggregation_type);
	}
}

void PageTable::print_second_level_table_info(uint32_t * table, uintptr_t base) {
	uintptr_t aggregation_start;
	uint32_t aggregation_count = 0;
	AggregationTypes aggregation_type;
	
	for (uint32_t i = 0; i < SECOND_LEVEL_ENTRIES; i++){
		uint32_t &second_level_entry = table[i];
		uintptr_t page_base = base + i * PAGE_SIZE;
		
		if (!(second_level_entry & 0x2)){
			if (second_level_entry & 0x4){
				//section is reserved
				if (aggregation_count > 0 && aggregation_type != AggregationTypes::Reserved){
					page_aggregation_display(aggregation_start, aggregation_count, aggregation_type);
				}
				
				if (aggregation_count++ == 0){
					aggregation_start = page_base;
					aggregation_type = AggregationTypes::Reserved;
				}
			} else {
				//unmapped
				if (aggregation_count > 0 && aggregation_type != AggregationTypes::Unmapped){
					page_aggregation_display(aggregation_start, aggregation_count, aggregation_type);
				}
				
				if (aggregation_count++ == 0){
					aggregation_start = page_base;
					aggregation_type = AggregationTypes::Unmapped;
				}
			}
		} else {
			if (aggregation_count > 0){
				page_aggregation_display(aggregation_start, aggregation_count, aggregation_type);
			}
			
			uintptr_t address = (second_level_entry & 0xfffff000);
			
			bool nx = second_level_entry & 0x1;
			
			uart_puts("\t");
			uart_puthex(page_base);
			uart_puts("\tpage mapped to ");
			uart_puthex(address);
			if (nx){
				uart_puts(" NX");
			}
			uart_puts("\r\n");
		}
	}
	
	if (aggregation_count > 0){
		page_aggregation_display(aggregation_start, aggregation_count, aggregation_type);
	}
}

uint32_t get_num_allocation_units(size_t bytes, AllocationGranularity granularity) {
	switch (granularity){
		case AllocationGranularity::Page:
			return bytes / PAGE_SIZE + ((bytes & (PAGE_SIZE-1)) ? 1 : 0);
		case AllocationGranularity::Section:
			return bytes / SECTION_SIZE + ((bytes & (SECTION_SIZE-1)) ? 1 : 0);
		case AllocationGranularity::Supersection:
			return bytes / SUPERSECTION_SIZE + ((bytes & (SUPERSECTION_SIZE-1)) ? 1 : 0);
		default:
			panic(PanicCodes::IncompatibleParameter);
	}
}

void PagingManager::SetLowerPageTable(const PageTable &table) {
	uintptr_t ttb = (uintptr_t)table.first_level_table & 0xffffc000;
	asm volatile("mcr p15, 0, %[ttb], c2, c0, 0" : : [ttb] "r" (ttb));
}

void PagingManager::SetUpperPageTable(const PageTable &table) {
	uintptr_t ttb = (uintptr_t)table.first_level_table & 0xffffc000;
	asm volatile("mcr p15, 0, %[ttb], c2, c0, 1" : : [ttb] "r" (ttb));
}

void PagingManager::SetPagingMode(bool lower_enable, bool upper_enable){
	uint32_t control = 0x00000001;
	if (!lower_enable) control |= 0x10;
	if (!upper_enable) control |= 0x20;
	
	asm volatile ("mcr p15, 0, %[control], c2, c0, 2" : : [control] "r" (control));
	
	//also set the domains
	asm volatile ("mcr p15, 0, %[domains], c3, c0, 0" : : [domains] "r" (0xffffffff));
}

void PagingManager::EnablePaging(){
	//some sort of identity mapping MUST be set up before calling this
	
	//disable caches
	uint32_t status;
	asm volatile ("mrc p15, 0, %[status], c1, c0, 0" : [status] "=r" (status));
	status = status & 0xffffeffb;
	asm volatile ("mcr p15, 0, %[status], c1, c0, 0" : : [status] "r" (status));
	
	//invalidate icache
	asm volatile ("mcr p15, 0, %[dummy], c7, c5, 0" : : [dummy] "r" (0));
	
	//invalidate tlb
	asm volatile ("mcr p15, 0, %[dummy], c8, c7, 0" : : [dummy] "r" (0));
	
	//memory barrier
	std::atomic_thread_fence(std::memory_order_seq_cst);
	
	//enable mmu ARMv6 mode and paging
	status = status | 0x00800001;
	asm volatile ("mcr p15, 0, %[status], c1, c0, 0" : : [status] "r" (status));
}

