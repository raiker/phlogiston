#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void delay(int32_t count);
size_t strlen(const char* str);
void memcpy(uintptr_t dest, uintptr_t src, size_t count);
void memset(uintptr_t dest, uint8_t ch, size_t count);

