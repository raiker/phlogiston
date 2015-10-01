#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void uart_init();
void uart_putc(unsigned char byte);
unsigned char uart_getc();
void uart_write(const unsigned char* buffer, size_t size);
void uart_puts(const char* str);
void uart_puthex(uint32_t x);

