#pragma once
#include "common.h"

//const uint32_t MMIO_BASE = 0x20000000;
const uint32_t MMIO_BASE = 0x3f000000;
const uint32_t GPIO_BASE = MMIO_BASE + 0x200000;

void mmio_write(uint32_t reg, uint32_t data);
uint32_t mmio_read(uint32_t reg);
