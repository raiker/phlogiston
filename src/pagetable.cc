#include "pagetable.h"
#include "panic.h"
#include "uart.h"

#include <atomic>
#include <algorithm>

enum class AggregationTypes {
	Unmapped,
	Reserved,
};

static uint32_t get_allocation_pages(AllocationGranularity granularity){
	switch (granularity){
		case AllocationGranularity::Page:
			return 1;
		case AllocationGranularity::Section:
			return 256;
//		case AllocationGranularity::Supersection:
//			return 4096;
		default:
			panic(PanicCodes::IncompatibleParameter);
	}
}

static BlockState get_state_from_descriptor(uint32_t descriptor){
	if ((descriptor & 0x7) == 0){
		return BlockState::Free;
	} else if ((descriptor & 0x7) == 4){
		return BlockState::Reserved;
	} else {
		return BlockState::Committed;
	}
}

static void section_aggregation_display(
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

static void page_aggregation_display(
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

uint32_t get_num_allocation_units(size_t bytes, AllocationGranularity granularity) {
	switch (granularity){
		case AllocationGranularity::Page:
			return bytes / PAGE_SIZE + ((bytes & (PAGE_SIZE-1)) ? 1 : 0);
		case AllocationGranularity::Section:
			return bytes / SECTION_SIZE + ((bytes & (SECTION_SIZE-1)) ? 1 : 0);
//		case AllocationGranularity::Supersection:
//			return bytes / SUPERSECTION_SIZE + ((bytes & (SUPERSECTION_SIZE-1)) ? 1 : 0);
		default:
			panic(PanicCodes::IncompatibleParameter);
	}
}

//IMPLEMENTATION INFO
//bit 2 in a "Translation fault" descriptor (i.e. bits [1:0] == 2'b00) represents:
// 0 = page is not reserved
// 1 = page is reserved

//********************
//***PUBLIC METHODS***
//********************

PageTable::PageTable(PageAlloc &_page_alloc, bool is_supervisor, bool is_reference_counted) :
	page_alloc(_page_alloc)
{
	first_level_table = (uint32_t*)page_alloc.alloc(4); //4 pages for both modes
	
	reference_counted = is_reference_counted;
	
	if (is_supervisor){
		//supervisor (4k entries)
		first_level_start_entry = FIRST_LEVEL_USER_ENTRIES;
		first_level_num_entries = FIRST_LEVEL_SUPERVISOR_ENTRIES;
	} else {
		//user (2k entries)
		first_level_start_entry = 0;
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
			*descriptor.get_value() |= 0x1; //NX bit
		}
	}*/
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

Result<uintptr_t, PageTableErrors> PageTable::reserve(uint32_t units, AllocationGranularity granularity){
	auto lock = spinlock_cs.acquire();
	
	switch (granularity){
		case AllocationGranularity::Page:
			return reserve_pages(units);
		case AllocationGranularity::Section:
			return reserve_sections(units);
//		case AllocationGranularity::Supersection:
//			return reserve_supersections(units);
		default:
			panic(PanicCodes::IncompatibleParameter);
	}
}

Result<uintptr_t, PageTableErrors> PageTable::reserve(uintptr_t address, uint32_t units, AllocationGranularity granularity){
	auto lock = spinlock_cs.acquire();
	
	switch (granularity){
		case AllocationGranularity::Page:
			return reserve_pages(address, units);
		case AllocationGranularity::Section:
			return reserve_sections(address, units);
//		case AllocationGranularity::Supersection:
//			return reserve_supersections(address, units);
		default:
			panic(PanicCodes::IncompatibleParameter);
	}
}

Result<uintptr_t, PageTableErrors> PageTable::reserve_allocate(uint32_t units, AllocationGranularity granularity){
	//doesn't acquire lock; convenience method
	
	Result<uintptr_t, PageTableErrors> reservation = reserve(units, granularity);
	if (reservation.is_success()){
		auto alloc_result = allocate(reservation.get_value(), units, granularity);
		if (alloc_result.is_error()){
			return Result<uintptr_t, PageTableErrors>::failure(alloc_result.get_error());
		}
	}
	return reservation;
}

Result<uintptr_t, PageTableErrors> PageTable::reserve_allocate(uintptr_t address, uint32_t units, AllocationGranularity granularity){
	//doesn't acquire lock; convenience method
	
	Result<uintptr_t, PageTableErrors> reservation = reserve(address, units, granularity);
	if (reservation.is_success()){
		auto alloc_result = allocate(reservation.get_value(), units, granularity);
		if (alloc_result.is_error()){
			return Result<uintptr_t, PageTableErrors>::failure(alloc_result.get_error());
		}
	}
	return reservation;
}

ErrorResult<PageTableErrors> PageTable::allocate(uintptr_t virtual_address, uint32_t units, AllocationGranularity granularity){
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
		ErrorResult<PageTableErrors> commit_result = ErrorResult<PageTableErrors>::success();
		
		switch (granularity){
			case AllocationGranularity::Page:
				commit_result = commit_page(map_address, physical_address);
				break;
			case AllocationGranularity::Section:
				commit_result = commit_section(map_address, physical_address);
				break;
			default:
				panic(PanicCodes::IncompatibleParameter);
		}
		
		if (commit_result.is_error()){
			uart_puts("failure\r\n");
			page_alloc.ref_release(physical_address, get_allocation_pages(granularity)); //free the allocated memory
			
			//clean up previously-allocated pages
			if (granularity == AllocationGranularity::Page){
				decommit_pages(virtual_address, i); //assume success
			} else if (granularity == AllocationGranularity::Section){
				decommit_sections(virtual_address, i);
			}
			return commit_result;
		}
	}
	
	return ErrorResult<PageTableErrors>::success();
}

ErrorResult<PageTableErrors> PageTable::map(uintptr_t virtual_address, uintptr_t physical_address, uint32_t units, AllocationGranularity granularity) {
	auto lock = spinlock_cs.acquire();
	
	for (uint32_t i = 0; i < units; i++){
		uintptr_t offset = i * get_allocation_pages(granularity) * PAGE_SIZE;
		
		//map it
		ErrorResult<PageTableErrors> commit_result = ErrorResult<PageTableErrors>::success();
		
		switch (granularity){
			case AllocationGranularity::Page:
				commit_result = commit_page(virtual_address + offset, physical_address + offset);
				break;
			case AllocationGranularity::Section:
				commit_result = commit_section(virtual_address + offset, physical_address + offset);
				break;
//			case AllocationGranularity::Supersection:
//				commit_result = commit_supersection(virtual_address + offset, physical_address + offset);
				break;
			default:
				panic(PanicCodes::IncompatibleParameter);
		}
		
		if (commit_result.is_error()) {
			//clean up previously-allocated pages
			if (granularity == AllocationGranularity::Page){
				decommit_pages(virtual_address, i); //assume success
			} else if (granularity == AllocationGranularity::Section){
				decommit_sections(virtual_address, i);
			}
			return commit_result;
		}
		
		if (reference_counted){
			//bump reference counts
			page_alloc.ref_acquire(physical_address + offset, get_allocation_pages(granularity));
		}
	}
	
	return ErrorResult<PageTableErrors>::success();
}

//follows redesign
ErrorResult<PageTableErrors> PageTable::deallocate(uintptr_t virtual_address, uint32_t units, AllocationGranularity granularity){
	auto lock = spinlock_cs.acquire();
	
	switch (granularity){
		case AllocationGranularity::Page:
			virtual_address &= 0xfffff000;
			return decommit_pages(virtual_address, units);
		case AllocationGranularity::Section:
			virtual_address &= 0xfff00000;
			return decommit_sections(virtual_address, units);
		default:
			panic(PanicCodes::IncompatibleParameter);
	}
}

//follows redesign
ErrorResult<PageTableErrors> PageTable::release(uintptr_t virtual_address, uint32_t units, AllocationGranularity granularity){
	auto lock = spinlock_cs.acquire();
	
	switch (granularity){
		case AllocationGranularity::Page:
			virtual_address &= 0xfffff000;
			return release_pages(virtual_address, units);
		case AllocationGranularity::Section:
			virtual_address &= 0xfff00000;
			return release_sections(virtual_address, units);
		default:
			panic(PanicCodes::IncompatibleParameter);
	}}

Result<BlockState, PageTableErrors> PageTable::get_block_state(uintptr_t virtual_address, AllocationGranularity granularity) {
	auto lock = spinlock_cs.acquire();
	
	switch (granularity){
		case AllocationGranularity::Page:
			{
				Result<uint32_t*, PageTableErrors> section_descriptor = get_section_descriptor(virtual_address, false);
				if (section_descriptor.is_success()){
					//it's mapped as a section
					return Result<BlockState, PageTableErrors>::success(get_state_from_descriptor(*section_descriptor.get_value()));
				}
				//it's mapped as a second-level table
				Result<uint32_t*, PageTableErrors> page_descriptor = get_page_descriptor(virtual_address);
				if (page_descriptor.is_error()){
					return Result<BlockState, PageTableErrors>::failure(page_descriptor.get_error());
				}
				
				return Result<BlockState, PageTableErrors>::success(get_state_from_descriptor(*page_descriptor.get_value()));
			}
		case AllocationGranularity::Section:
			{
				Result<uint32_t*, PageTableErrors> section_descriptor = get_section_descriptor(virtual_address, false);
				if (section_descriptor.is_success()){
					//it's mapped as a section
					return Result<BlockState, PageTableErrors>::success(get_state_from_descriptor(*section_descriptor.get_value()));
				} else {
					return Result<BlockState, PageTableErrors>::failure(section_descriptor.get_error());
				}
			}
/*		case AllocationGranularity::Supersection:
			{
				virtual_address &= 0xff000000;
				bool all_free = true;
				bool all_reserved = true;
				bool all_committed = true;
				
				for (uint32_t i = 0; i < 16; i++){
					Result<uint32_t*, PageTableErrors> section_descriptor = get_section_descriptor(virtual_address + i * SECTION_SIZE, false);
					if (section_descriptor.is_success()){
						//it's mapped as a supersection
						if ((*section_descriptor.get_value() & 0x7) == 0){
							all_reserved = false;
							all_committed = false;
						} else if ((*section_descriptor.get_value() & 0x7) == 4){
							all_free = false;
							all_committed = false;
						} else {
							all_free = false;
							all_reserved = false;
						}
					} else {
						return Result<BlockState, PageTableErrors>::failure(section_descriptor.get_error());
					}
				}
				
				if (all_free){
					return Result<BlockState, PageTableErrors>::success(BlockState::Free);
				} else if (all_reserved){
					return Result<BlockState, PageTableErrors>::success(BlockState::Reserved);
				} else if (all_committed){
					return Result<BlockState, PageTableErrors>::success(BlockState::Committed);
				} else {
					return Result<BlockState, PageTableErrors>::failure();
				}
			}*/
		default:
			return Result<BlockState, PageTableErrors>::failure(PageTableErrors::InconsistentSupersection);
	}
}

Result<uintptr_t, PageTableErrors> PageTable::virtual_to_physical(uintptr_t virtual_address) {
	auto lock = spinlock_cs.acquire();
	
	return virtual_to_physical_internal(virtual_address);
}

Result<uintptr_t, PageTableErrors> PageTable::physical_to_virtual(uintptr_t physical_address) {
	auto lock = spinlock_cs.acquire();
	
	return physical_to_virtual_internal(physical_address);
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

//*********************
//***PRIVATE METHODS***
//*********************

Result<uintptr_t, PageTableErrors> PageTable::virtual_to_physical_internal(uintptr_t virtual_address) {
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
		return Result<uintptr_t, PageTableErrors>::failure(PageTableErrors::OutOfBounds);
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
					return Result<uintptr_t, PageTableErrors>::success(address);
				} else {
					//page is unallocated or reserved (we don't use large pages)
					return Result<uintptr_t, PageTableErrors>::failure(PageTableErrors::AddressNotMapped);
				}
			}
			break;
		case 2:
			//section or supersection
			if (first_level_entry & (1<<18)){
				//supersection
				uintptr_t address = (first_level_entry & 0xff000000) | (virtual_address & 0x00ffffff);
				return Result<uintptr_t, PageTableErrors>::success(address);
			} else {
				//regular section
				uintptr_t address = (first_level_entry & 0xfff00000) | (virtual_address & 0x000fffff);
				return Result<uintptr_t, PageTableErrors>::success(address);
			}
			break;
		default:
			//memory is not currently committed
			return Result<uintptr_t, PageTableErrors>::failure(PageTableErrors::AddressNotMapped);
	}
}

Result<uintptr_t, PageTableErrors> PageTable::physical_to_virtual_internal(uintptr_t physical_address) {
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
								return Result<uintptr_t, PageTableErrors>::success(address);
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
						return Result<uintptr_t, PageTableErrors>::success(address);
					}
				} else {
					//regular section
					if ((first_level_entry & 0xfff00000) == (physical_address & 0xfff00000)){
						//match
						uintptr_t address = (first_level_entry & 0xfff00000) | (physical_address & 0x000fffff);
						return Result<uintptr_t, PageTableErrors>::success(address);
					}
				}
				break;
			default:
				//memory is not currently committed
				;
		}
	}
	
	return Result<uintptr_t, PageTableErrors>::failure(PageTableErrors::AddressNotMapped);
}

uint32_t * PageTable::create_second_level_table() {
	uint32_t * second_level_table = (uint32_t*)page_alloc.alloc(1); //4 times as much space as we need...
	
	for (uint32_t i = 0; i < SECOND_LEVEL_ENTRIES; i++) {
		second_level_table[i] = 0x00000000;
	}
	
	return second_level_table;
}

uint32_t * PageTable::get_first_level_table_address() {
	return first_level_table;
}

uint32_t * PageTable::get_second_level_table_address(uintptr_t physical_base_address){
	return (uint32_t*)physical_base_address;
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

//returns the set of properties displayed by all of the pages in the range
//if allow_breaking_sections is false, recursing into a section returns all false
//virtual_address must be page-aligned
MultiBlockState PageTable::get_page_state(uintptr_t virtual_address, uint32_t num_pages, bool allow_breaking_sections) {
	MultiBlockState retval;
	
	uint32_t num_unchecked_pages = num_pages;
	uintptr_t check_base = virtual_address;
	
	while (num_unchecked_pages > 0){
		uint32_t pages_in_rest_of_section = ((check_base & 0xfff00000) - check_base + SECTION_SIZE) / PAGE_SIZE;
		uint32_t pages_to_check = std::min(num_unchecked_pages, pages_in_rest_of_section);
		
		retval &= get_page_state_inside_section(check_base, pages_to_check, allow_breaking_sections);
		
		num_unchecked_pages -= pages_to_check;
		check_base = (check_base & 0xfff00000) + SECTION_SIZE;
	}
	
	return retval;
}

//as above, but all pages are in the same section
//virtual_address must be page-aligned
MultiBlockState PageTable::get_page_state_inside_section(uintptr_t virtual_address, uint32_t num_pages, bool allow_breaking_sections) {
	auto result = get_section_descriptor(virtual_address, true);
	
	if (result.is_error()) return MultiBlockState {false, false, false};
	
	if ((*result.get_value() & 0x3) == 1) {
		//second-level page table
		MultiBlockState retval;
		for (uint32_t i = 0; i < num_pages; i++){
			result = get_page_descriptor(virtual_address + i * PAGE_SIZE);
			
			if (result.is_error()) {
				retval &= MultiBlockState {false, false, false};
			}
			
			retval &= get_state_from_descriptor(*result.get_value());
		}
		return retval;
	} else {
		//it's a fully-mapped section
		if (allow_breaking_sections){
			return get_state_from_descriptor(*result.get_value());
		} else {
			return MultiBlockState {false, false, false};
		}
	}
}

//returns the set of properties displayed by all of the sections in the range
//if allow_combining_pages is false, recursing into a second-level table returns all false
//virtual_address must be section-aligned
MultiBlockState PageTable::get_section_state(uintptr_t virtual_address, uint32_t num_sections, bool allow_combining_pages) {
	MultiBlockState retval;
	
	for (uint32_t i = 0; i < num_sections; i++){
		auto result = get_section_descriptor(virtual_address + i * SECTION_SIZE, allow_combining_pages);
		
		if (result.is_error()) return MultiBlockState {false, false, false};
		
		if ((*result.get_value() & 0x3) == 1) {
			//second-level page table
			retval &= get_page_state_inside_section(virtual_address + i * SECTION_SIZE, SECTION_SIZE / PAGE_SIZE, true);
		} else {
			//it's a fully-mapped section
			if (allow_combining_pages){
				retval &= get_state_from_descriptor(*result.get_value());
			} else {
				return MultiBlockState {false, false, false};
			}
		}
	}
	
	return retval;
}

Result<uintptr_t, PageTableErrors> PageTable::reserve_pages(uint32_t num_pages){
	uint32_t * first_level_table = get_first_level_table_address();
	
	//trawl through all the second-level page tables looking for a space
	for (uint32_t i = first_level_start_entry; i < first_level_num_entries; i++){
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
					
					return Result<uintptr_t, PageTableErrors>::success(i * SECTION_SIZE + start_index * PAGE_SIZE);
				}
			}
		}
	}
	
	//if we get here, there were no free pages
	//create a new second-level table
	for (uint32_t i = first_level_start_entry; i < first_level_num_entries; i++){
		uint32_t & first_level_entry = first_level_table[i];
		if ((first_level_entry & 0x7) == 0x0){
			//it's an unreserved free section
			
			//TODO: fix this
			uint32_t * second_level_table = get_second_level_table_address((uint32_t)create_second_level_table());
			first_level_table[i] = (uint32_t)second_level_table | 0x1 | (SUPERVISOR_DOMAIN << 5);
			
			for (uint32_t j = 0; j < num_pages; j++){
				second_level_table[j] = 0x00000004;
			}
			
			return Result<uintptr_t, PageTableErrors>::success(i * SECTION_SIZE);
		}
	}
	
	//if we get here, we have insufficient address space left (i.e. it's all been reserved (or allocated... ^_^))
	return Result<uintptr_t, PageTableErrors>::failure(PageTableErrors::MemorySpaceExhausted);
}

Result<uintptr_t, PageTableErrors> PageTable::reserve_sections(uint32_t num_sections) {
	uint32_t * first_level_table = get_first_level_table_address();
	
	uint32_t contiguous_free_sections = 0;
	
	for (uint32_t i = first_level_start_entry; i < first_level_num_entries; i++){
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
			
			return Result<uintptr_t, PageTableErrors>::success(start_index * SECTION_SIZE);
		}
	}
	
	//if we get here, we have insufficient address space left (i.e. it's all been reserved (or allocated... ^_^))
	return Result<uintptr_t, PageTableErrors>::failure(PageTableErrors::MemorySpaceExhausted);
}

/*Result<uintptr_t, PageTableErrors> PageTable::reserve_supersections(uint32_t num_supersections) {
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
			
			return Result<uintptr_t, PageTableErrors>::success(start_index * SECTION_SIZE);
		}
	}
	
	//if we get here, we have no address space left (i.e. it's all been reserved (or allocated... ^_^))
	return Result<uintptr_t, PageTableErrors>::failure(PageTableErrors::MemorySpaceExhausted);
}*/

Result<uintptr_t, PageTableErrors> PageTable::reserve_pages(uintptr_t base, uint32_t num_pages) {
	base &= 0xfffff000;
	
	//check the pages are all reservable
	uint32_t num_unchecked_pages = num_pages;
	uintptr_t check_base = base;
	
	while (num_unchecked_pages > 0){
		uint32_t pages_in_rest_of_section = ((check_base & 0xfff00000) - check_base + SECTION_SIZE) / PAGE_SIZE;
		uint32_t pages_to_check = std::min(num_unchecked_pages, pages_in_rest_of_section);
		
		if (!check_section_partially_reservable(check_base, pages_to_check)){
			return Result<uintptr_t, PageTableErrors>::failure(PageTableErrors::SomeBlocksNotFree);
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
	
	return Result<uintptr_t, PageTableErrors>::success(base);
}

Result<uintptr_t, PageTableErrors> PageTable::reserve_sections(uintptr_t base, uint32_t num_sections) {
	base &= 0xfff00000;
	
	for (uint32_t i = 0; i < num_sections; i++){
		auto result = get_section_descriptor(base + i * SECTION_SIZE, false);
		if (result.is_error() || (*result.get_value() & 0x7)){
			return Result<uintptr_t, PageTableErrors>::failure(PageTableErrors::SomeBlocksNotFree);
		}
	}
	
	for (uint32_t i = 0; i < num_sections; i++){
		auto result = get_section_descriptor(base + i * SECTION_SIZE, false);
		*result.get_value() = 0x00000004;
	}
	
	return Result<uintptr_t, PageTableErrors>::success(base);
}

/*Result<uintptr_t, PageTableErrors> PageTable::reserve_supersections(uintptr_t base, uint32_t num_supersections) {
	base &= 0xff000000;
	
	for (uint32_t i = 0; i < num_supersections * 16; i++){
		auto result = get_section_descriptor(base + i * SECTION_SIZE, false);
		if (!result.is_success || *result.get_value() & 0x7){
			return Result<uintptr_t, PageTableErrors>::failure(PageTableErrors::SomeBlocksNotFree);
		}
	}
	
	for (uint32_t i = 0; i < num_supersections * 16; i++){
		auto result = get_section_descriptor(base + i * SECTION_SIZE, false);
		*result.get_value() = 0x00000004;
	}
	
	return Result<uintptr_t, PageTableErrors>::success(base);
}*/

//checks whether num_pages pages can be reserved in the section enclosing base, starting from base
bool PageTable::check_section_partially_reservable(uintptr_t base, uint32_t num_pages) {
	auto result = get_section_descriptor(base, true);
	
	if (result.is_error()) return false;
	
	//if result is a free section, we're fine
	if ((*result.get_value() & 0x7) == 0) return true;
	
	//second-level page table
	if ((*result.get_value() & 0x3) == 1) {
		for (uint32_t i = 0; i < num_pages; i++){
			result = get_page_descriptor(base + i * PAGE_SIZE);
			
			if (result.is_error() || (*result.get_value() & 0x7)) return false;
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
	
	if ((*result.get_value() & 0x7) == 0) {
		//create new second-level table
		//TODO: fix this
		uint32_t * new_table = create_second_level_table();
		*result.get_value() = (uint32_t)new_table | 0x1 | (SUPERVISOR_DOMAIN << 5);
		second_level_table = get_second_level_table_address((uintptr_t)new_table);
	} else {
		second_level_table = get_second_level_table_address(*result.get_value() & 0xfffffc00);
	}
	
	uint32_t start_index = (base & 0x000ff000) >> 12;
	
	for (uint32_t i = 0; i < num_pages; i++){
		second_level_table[start_index + i] = 0x00000004;
	}
}

ErrorResult<PageTableErrors> PageTable::commit_page(uintptr_t virtual_address, uintptr_t physical_address){
	//TODO: Permission bits
	virtual_address &= 0xfffff000;
	physical_address &= 0xfffff000;
	
	auto result = get_page_descriptor(virtual_address);
	
	if (result.is_success()){
		//in order to be committed, the page needs to be reserved already
		if ((*result.get_value() & 0x7) == 0x4){
			//reserved but not committed yet
			*result.get_value() = physical_address | 0x00000002;
			return ErrorResult<PageTableErrors>::success();
		}
	}
	return ErrorResult<PageTableErrors>::failure(result.get_error());
}

ErrorResult<PageTableErrors> PageTable::commit_section(uintptr_t virtual_address, uintptr_t physical_address){
	//TODO: Permission bits
	virtual_address &= 0xfff00000;
	physical_address &= 0xfff00000;
	
	auto result = get_section_descriptor(virtual_address, false);
	
	if (result.is_success()){
		//in order to be committed, the section needs to be reserved already
		if ((*result.get_value() & 0x7) == 0x4){
			//reserved but not committed yet
			*result.get_value() = physical_address | 0x00000002;
			return ErrorResult<PageTableErrors>::success();;
		}
	}
	return ErrorResult<PageTableErrors>::failure(result.get_error());
}

/*ErrorResult<PageTableErrors> PageTable::commit_supersection(uintptr_t virtual_address, uintptr_t physical_address){
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
			if ((result.get_value()[i] & 0x07) != 0x04) return false;
		}
		for (uint32_t i = 0; i < 16; i++){
			result.get_value()[i] = physical_address | 0x00040002;
		}
		return ErrorResult<PageTableErrors>::success();
	}
	return ErrorResult<PageTableErrors>::failure(result.get_error());
}*/

//virtual_address is page-aligned
ErrorResult<PageTableErrors> PageTable::decommit_pages(uintptr_t virtual_address, uint32_t num_pages) {
	MultiBlockState page_state = get_page_state(virtual_address, num_pages, false);
	
	if (!page_state.is_committed){
		return ErrorResult<PageTableErrors>::failure(PageTableErrors::SomeBlocksNotCommitted);
	}
	
	for (uint32_t i = 0; i < num_pages; i++){
		auto result = get_page_descriptor(virtual_address + i * PAGE_SIZE);
		
		if (result.is_error()){
			panic(PanicCodes::AssertionFailure);
		}
		
		uintptr_t physical_address = *result.get_value() & 0xfffff000;
		page_alloc.ref_release(physical_address);
		*result.get_value() = 0x00000004;
	}
	return ErrorResult<PageTableErrors>::success();
}

//virtual_address is section-aligned
ErrorResult<PageTableErrors> PageTable::decommit_sections(uintptr_t virtual_address, uint32_t num_sections){
	MultiBlockState section_state = get_section_state(virtual_address, num_sections, false);
	
	if (!section_state.is_committed){
		return ErrorResult<PageTableErrors>::failure(PageTableErrors::SomeBlocksNotCommitted);
	}
	
	for (uint32_t i = 0; i < num_sections; i++){
		auto result = get_section_descriptor(virtual_address + i * SECTION_SIZE, false);
		
		if (result.is_error()){
			panic(PanicCodes::AssertionFailure);
		}
		
		uintptr_t physical_address = *result.get_value() & 0xfff00000;
		page_alloc.ref_release(physical_address, SECTION_SIZE / PAGE_SIZE);
		*result.get_value() = 0x00000004;
	}
	return ErrorResult<PageTableErrors>::success();
}

//not allowed to release sections
//virtual_address is page-aligned
ErrorResult<PageTableErrors> PageTable::release_pages(uintptr_t virtual_address, uint32_t num_pages){
	MultiBlockState page_state = get_page_state(virtual_address, num_pages, false);
	
	if (!page_state.is_reserved){
		return ErrorResult<PageTableErrors>::failure(PageTableErrors::SomeBlocksNotReserved);
	}
	
	for (uint32_t i = 0; i < num_pages; i++){
		auto result = get_page_descriptor(virtual_address + i * PAGE_SIZE);
		
		if (result.is_error()){
			panic(PanicCodes::AssertionFailure);
		}
		
		*result.get_value() = 0x00000000;
	}
	return ErrorResult<PageTableErrors>::success();
}

//allowed to release pages as well
//virtual_address is section-aligned
ErrorResult<PageTableErrors> PageTable::release_sections(uintptr_t virtual_address, uint32_t num_sections){
	MultiBlockState section_state = get_section_state(virtual_address, num_sections, true);
	
	if (!section_state.is_reserved){
		return ErrorResult<PageTableErrors>::failure(PageTableErrors::SomeBlocksNotReserved);
	}
	
	for (uint32_t i = 0; i < num_sections; i++){
		uintptr_t section_base = virtual_address + i * SECTION_SIZE;
		auto result = get_section_descriptor(section_base, true);
		
		if (result.is_error()){
			panic(PanicCodes::AssertionFailure);
		}
		
		if ((*result.get_value() & 0x3) == 0x1){
			//it's a second-level table
			auto result2 = release_pages(section_base, SECTION_SIZE / PAGE_SIZE);
			if (result2.is_error()){
				return result2;
			}
		} else {
			*result.get_value() = 0x00000000;
		}
	}
	return ErrorResult<PageTableErrors>::success();
}

Result<uint32_t*, PageTableErrors> PageTable::get_page_descriptor(uintptr_t virtual_address) {
	uint32_t first_level_index = virtual_address >> 20;
	
	if (first_level_index >= first_level_num_entries) {
		return Result<uint32_t*, PageTableErrors>::failure(PageTableErrors::OutOfBounds);
	}
	
	uint32_t & first_level_entry = get_first_level_table_address()[first_level_index];
	
	if ((first_level_entry & 0x3) == 1) {
		uint32_t * second_level_table = get_second_level_table_address(first_level_entry & 0xfffffc00);
		uint32_t second_level_index = (virtual_address >> 12) & 0xff;
	
		return Result<uint32_t*, PageTableErrors>::success(&second_level_table[second_level_index]);
	}
	
	return Result<uint32_t*, PageTableErrors>::failure(PageTableErrors::NotMappedAsPage);
}

Result<uint32_t*, PageTableErrors> PageTable::get_section_descriptor(uintptr_t virtual_address, bool allow_second_level) {
	uint32_t first_level_index = virtual_address >> 20;
	
	if (first_level_index >= first_level_num_entries) {
		return Result<uint32_t*, PageTableErrors>::failure(PageTableErrors::NotMappedAsPage);
	}
	
	uint32_t & first_level_entry = get_first_level_table_address()[first_level_index];
	
	if ((first_level_entry & 0x3) == 1 && !allow_second_level) {
		return Result<uint32_t*, PageTableErrors>::failure(PageTableErrors::NotMappedAsSection);
	} else {
		return Result<uint32_t*, PageTableErrors>::success(&first_level_entry);
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

