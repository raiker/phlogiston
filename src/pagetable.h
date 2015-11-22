#pragma once

#include "common.h"
#include "spinlock.h"
#include "page_alloc.h"

struct SecondLevelTableAddr {
	uintptr_t physical_addr;
	uint32_t (* virtual_addr)[];
};

const uint32_t FIRST_LEVEL_SUPERVISOR_ENTRIES = 0x1000;
const uint32_t FIRST_LEVEL_USER_ENTRIES = 0x800;
const uint32_t SECOND_LEVEL_ENTRIES = 0x100;
const uint32_t MAX_SECOND_LEVEL_TABLES = PAGE_SIZE / sizeof(SecondLevelTableAddr);

const uint32_t SUPERVISOR_DOMAIN = 0;

//first level page tables are 64KiB each
//second level page tables are 1KiB each

enum class AllocationGranularity {
	Page,
	Section,
};

enum class BlockState {
	Free,
	Reserved,
	Committed,
};

struct MultiBlockState {
	bool is_free : 1, is_reserved : 1, is_committed : 1;
	
	MultiBlockState(bool _is_free, bool _is_reserved, bool _is_committed) :
		is_free(_is_free),
		is_reserved(_is_reserved),
		is_committed(_is_committed)
	{ }
	
	MultiBlockState(const BlockState & bs){
		is_free = (bs == BlockState::Free);
		is_reserved = (bs == BlockState::Reserved);
		is_committed = (bs == BlockState::Committed);
	}
	
	MultiBlockState() :
		is_free(true),
		is_reserved(true),
		is_committed(true)
	{ }
	
	MultiBlockState & operator &=(const MultiBlockState &rhs){
		this->is_free &= rhs.is_free;
		this->is_reserved &= rhs.is_reserved;
		this->is_committed &= rhs.is_committed;
		
		return *this;
	}
	
	/*MultiBlockState & operator &=(const BlockState &rhs){
		this->is_free &= (rhs == BlockState::Free);
		this->is_reserved &= (rhs == BlockState::Reserved);
		this->is_committed &= (rhs == BlockState::Committed);
		
		return *this;
	}*/
	
	friend MultiBlockState operator &(MultiBlockState lhs, const MultiBlockState &rhs){
		lhs &= rhs;
		return lhs;
	}
};

enum class PageTableErrors {
	SomeBlocksNotFree,
	SomeBlocksNotReserved,
	SomeBlocksNotCommitted,
	OutOfBounds,
	AddressNotMapped,
	InconsistentSupersection, //deprecated
	MemorySpaceExhausted,
	NotMappedAsPage,
	NotMappedAsSection,
};

uint32_t get_num_allocation_units(size_t bytes, AllocationGranularity granularity);

//Public functions are all thread-safe (non-reentrant)
//Spinlock must be acquired in every public method
//Spinlock is assumed to be held in every private method
//All inputs are checked in public methods (after redesign)
class PageTable {
private:
	friend class PagingManager;
	
	uint32_t * first_level_table;
	uint32_t first_level_start_entry; //0 for a user table, 2048 for a supervisor
	uint32_t first_level_num_entries; //should be 4096 or 2048
	bool reference_counted;
	Spinlock spinlock_cs;
	
	PageAlloc &page_alloc;

	Result<uintptr_t, PageTableErrors> virtual_to_physical_internal(uintptr_t virtual_address);
	Result<uintptr_t, PageTableErrors> physical_to_virtual_internal(uintptr_t physical_address);
	
	uint32_t * create_second_level_table();
	
	uint32_t * get_first_level_table_address();
	uint32_t * get_second_level_table_address(uintptr_t physical_base_address);
	
	void print_second_level_table_info(uint32_t * table, uintptr_t base);
	
	MultiBlockState get_page_state(uintptr_t virtual_address, uint32_t num_pages, bool allow_breaking_sections);
	MultiBlockState get_page_state_inside_section(uintptr_t virtual_address, uint32_t num_pages, bool allow_breaking_sections);
	MultiBlockState get_section_state(uintptr_t virtual_address, uint32_t num_sections, bool allow_combining_pages);
	
	Result<uintptr_t, PageTableErrors> reserve_pages(uint32_t num_pages);
	Result<uintptr_t, PageTableErrors> reserve_sections(uint32_t num_sections);
	//Result<uintptr_t, PageTableErrors> reserve_supersections(uint32_t num_supersections);

	Result<uintptr_t, PageTableErrors> reserve_pages(uintptr_t base, uint32_t num_pages);
	Result<uintptr_t, PageTableErrors> reserve_sections(uintptr_t base, uint32_t num_sections);
	//Result<uintptr_t, PageTableErrors> reserve_supersections(uintptr_t base, uint32_t num_supersections);
	
	bool check_section_partially_reservable(uintptr_t base, uint32_t num_pages);
	void reserve_pages_from_section(uintptr_t base, uint32_t num_pages);
	
	ErrorResult<PageTableErrors> commit_page(uintptr_t virtual_address, uintptr_t physical_address);
	ErrorResult<PageTableErrors> commit_section(uintptr_t virtual_address, uintptr_t physical_address);
	//ErrorResult<PageTableErrors> commit_supersection(uintptr_t virtual_address, uintptr_t physical_address);
	
	ErrorResult<PageTableErrors> decommit_pages(uintptr_t virtual_address, uint32_t num_pages);
	ErrorResult<PageTableErrors> decommit_sections(uintptr_t virtual_address, uint32_t num_sections);
	
	ErrorResult<PageTableErrors> release_pages(uintptr_t virtual_address, uint32_t num_pages);
	ErrorResult<PageTableErrors> release_sections(uintptr_t virtual_address, uint32_t num_sections);
	
	Result<uint32_t*, PageTableErrors> get_page_descriptor(uintptr_t virtual_address);
	Result<uint32_t*, PageTableErrors> get_section_descriptor(uintptr_t virtual_address, bool allow_second_level);
public:
	PageTable(PageAlloc &_page_alloc, bool is_supervisor, bool is_reference_counted = true);
	PageTable(const PageTable &other) = delete; //we don't want this to be copy-constructed
	~PageTable();
	
	Result<uintptr_t, PageTableErrors> reserve(uint32_t units, AllocationGranularity granularity);
	Result<uintptr_t, PageTableErrors> reserve(uintptr_t address, uint32_t units, AllocationGranularity granularity);
	
	Result<uintptr_t, PageTableErrors> reserve_allocate(uint32_t units, AllocationGranularity granularity);
	Result<uintptr_t, PageTableErrors> reserve_allocate(uintptr_t virtual_address, uint32_t units, AllocationGranularity granularity);
	
	ErrorResult<PageTableErrors> allocate(uintptr_t virtual_address, uint32_t units, AllocationGranularity granularity);
	
	ErrorResult<PageTableErrors> map(uintptr_t virtual_address, uintptr_t physical_address, uint32_t units, AllocationGranularity granularity);
	
	ErrorResult<PageTableErrors> deallocate(uintptr_t virtual_address, uint32_t units, AllocationGranularity granularity);
	
	ErrorResult<PageTableErrors> release(uintptr_t virtual_address, uint32_t units, AllocationGranularity granularity);

	Result<BlockState, PageTableErrors> get_block_state(uintptr_t virtual_address, AllocationGranularity granularity);
	
	Result<uintptr_t, PageTableErrors> virtual_to_physical(uintptr_t virtual_address);
	Result<uintptr_t, PageTableErrors> physical_to_virtual(uintptr_t physical_address);
	
	void print_table_info();
};

class PagingManager {
public:
	static void SetLowerPageTable(const PageTable &table);
	static void SetUpperPageTable(const PageTable &table);
	static void SetPagingMode(bool lower_enable, bool upper_enable);
	static void EnablePaging();
};


