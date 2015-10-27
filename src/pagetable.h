#pragma once

#include "common.h"
#include "spinlock.h"

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

enum AllocationGranularity {
	Page,
	Section,
	Supersection
};

class PrePagingPageTable {
	uint32_t first_level_num_entries; //should be 4096 or 2048
	uint32_t * first_level_table;
	Spinlock spinlock_cs;
	//SecondLevelTableAddr (* second_level_table_addrs)[];
	
	Result<uintptr_t> virtual_to_physical_internal(uintptr_t virtual_address);
	Result<uintptr_t> physical_to_virtual_internal(uintptr_t physical_address);
	
public:
	PrePagingPageTable(bool is_supervisor);
	~PrePagingPageTable();
	
	uintptr_t allocate(size_t bytes, AllocationGranularity granularity); //virtual address
	bool free(uintptr_t start, size_t bytes); //virtual address
	
	Result<uintptr_t> virtual_to_physical(uintptr_t virtual_address);
	Result<uintptr_t> physical_to_virtual(uintptr_t physical_address);
};

class PageTable {
};

