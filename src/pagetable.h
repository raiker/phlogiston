#pragma once

#include "common.h"

struct SecondLevelTableAddr {
	uintptr_t physical_addr;
	uint32_t (* virtual_addr)[];
};

const uint32_t FIRST_LEVEL_SUPERVISOR_ENTRIES = 0x1000;
const uint32_t FIRST_LEVEL_USER_ENTRIES = 0x800;
const uint32_t SECOND_LEVEL_ENTRIES = 0x100;
const uint32_t MAX_SECOND_LEVEL_TABLES = PAGE_SIZE / sizeof(SecondLevelTableAddr);

//first level page tables are 64KiB each
//second level page tables are 1KiB each

class PrePagingPageTable {
	uint32_t first_level_num_entries; //should be 4096 or 2048
	uint32_t * first_level_table;
	//SecondLevelTableAddr (* second_level_table_addrs)[];
	
public:
	PrePagingPageTable(bool is_supervisor);
	~PrePagingPageTable();
	
	Result<uintptr_t> virtual_to_physical(uintptr_t virtual_address);
	Result<uintptr_t> physical_to_virtual(uintptr_t physical_address);
};


