#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

const uint32_t PAGE_SIZE = 0x1000;

struct MemRange {
	uintptr_t start, size;
};


