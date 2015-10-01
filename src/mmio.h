#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void mmio_write(uint32_t reg, uint32_t data);
uint32_t mmio_read(uint32_t reg);
