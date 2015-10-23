#pragma once

#include "common.h"

struct SecondLevelTableAddr {
	uintptr_t physical_addr;
	uint32_t (* virtual_addr)[];
};

const uint32_t MAX_SECOND_LEVEL_TABLES = PAGE_SIZE / sizeof(SecondLevelTableAddr);

//first level page tables are 64KiB each
//second level page tables are 1KiB each

class PrePagingPageTable {
	uint32_t * first_level_table;
	//SecondLevelTableAddr (* second_level_table_addrs)[];
	
public:
	PageTable();
	~PageTable();
	
	Result<uintptr_t> virtual_to_physical(uintptr_t virtual_address);
	Result<uintptr_t> physical_to_virtual(uintptr_t physical_address);
};


