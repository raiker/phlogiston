#pragma once

#include "common.h"

class PageTable {
	uint32_t (* first_level_pa)[]; //physical address
	
	PageTable();
	~PageTable();
};


