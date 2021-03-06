#include "uart.h"

#include "mmio.h"
#include "utility.h"

enum
{
    // The GPIO registers base address.
    GPIO_BASE = 0x20200000,
 
    // The offsets for reach register.
 
    // Controls actuation of pull up/down to ALL GPIO pins.
    GPPUD = (GPIO_BASE + 0x94),
 
    // Controls actuation of pull up/down for specific GPIO pin.
    GPPUDCLK0 = (GPIO_BASE + 0x98),
 
    // The base address for UART.
    UART0_BASE = 0x20201000,
 
    // The offsets for reach register for the UART.
    UART0_DR     = (UART0_BASE + 0x00),
    UART0_RSRECR = (UART0_BASE + 0x04),
    UART0_FR     = (UART0_BASE + 0x18),
    UART0_ILPR   = (UART0_BASE + 0x20),
    UART0_IBRD   = (UART0_BASE + 0x24),
    UART0_FBRD   = (UART0_BASE + 0x28),
    UART0_LCRH   = (UART0_BASE + 0x2C),
    UART0_CR     = (UART0_BASE + 0x30),
    UART0_IFLS   = (UART0_BASE + 0x34),
    UART0_IMSC   = (UART0_BASE + 0x38),
    UART0_RIS    = (UART0_BASE + 0x3C),
    UART0_MIS    = (UART0_BASE + 0x40),
    UART0_ICR    = (UART0_BASE + 0x44),
    UART0_DMACR  = (UART0_BASE + 0x48),
    UART0_ITCR   = (UART0_BASE + 0x80),
    UART0_ITIP   = (UART0_BASE + 0x84),
    UART0_ITOP   = (UART0_BASE + 0x88),
    UART0_TDR    = (UART0_BASE + 0x8C),
};

void uart_init(){
	// Disable UART0.
	mmio_write(UART0_CR, 0x00000000);
	// Setup the GPIO pin 14 && 15.
 
	// Disable pull up/down for all GPIO pins & delay for 150 cycles.
	mmio_write(GPPUD, 0x00000000);
	delay(150);
 
	// Disable pull up/down for pin 14,15 & delay for 150 cycles.
	mmio_write(GPPUDCLK0, (1 << 14) | (1 << 15));
	delay(150);
 
	// Write 0 to GPPUDCLK0 to make it take effect.
	mmio_write(GPPUDCLK0, 0x00000000);
 
	// Clear pending interrupts.
	mmio_write(UART0_ICR, 0x7FF);
 
	// Set integer & fractional part of baud rate.
	// Divider = UART_CLOCK/(16 * Baud)
	// Fraction part register = (Fractional part * 64) + 0.5
	// UART_CLOCK = 3000000; Baud = 115200.
 
	// Divider = 3000000 / (16 * 115200) = 1.627 = ~1.
	// Fractional part register = (.627 * 64) + 0.5 = 40.6 = ~40.
	mmio_write(UART0_IBRD, 1);
	mmio_write(UART0_FBRD, 40);
 
	// Enable FIFO & 8 bit data transmissio (1 stop bit, no parity).
	mmio_write(UART0_LCRH, (1 << 4) | (1 << 5) | (1 << 6));
 
	// Mask all interrupts.
	mmio_write(UART0_IMSC, (1 << 1) | (1 << 4) | (1 << 5) | (1 << 6) |
	                       (1 << 7) | (1 << 8) | (1 << 9) | (1 << 10));
 
	// Enable UART0, receive & transfer part of UART.
	mmio_write(UART0_CR, (1 << 0) | (1 << 8) | (1 << 9));
}

void uart_putc(unsigned char byte){
	// Wait for UART to become ready to transmit.
	while ( mmio_read(UART0_FR) & (1 << 5) ) { }
	mmio_write(UART0_DR, byte);
}

unsigned char uart_getc(){
    // Wait for UART to have recieved something.
    while ( mmio_read(UART0_FR) & (1 << 4) ) { }
    return mmio_read(UART0_DR);
}
 
void uart_write(const unsigned char* buffer, size_t size){
	for ( size_t i = 0; i < size; i++ )
		uart_putc(buffer[i]);
}

void uart_puts(const char* str){
	uart_write((const unsigned char*) str, strlen(str));
}

void uart_put_hexnibble(uint8_t cx){
	if (cx < 10){
		uart_putc('0' + cx);
	} else {
		uart_putc('a' + cx - 10);
	}
}

void uart_puthex(uint32_t x){
	uart_putc('0');
	uart_putc('x');
	for (int i = 0; i < 8; i++){
		uint8_t cx = (x >> ((7 - i) << 2)) & 0xf;
		uart_put_hexnibble(cx);
	}
}

void uart_putdec(uint32_t x){
	bool anything_printed = false;
	
	for (uint32_t mod = 1000000000; mod > 0; mod /= 10){
		uint32_t digit = x / mod;
		x -= digit * mod;
		
		if (digit == 0 && !anything_printed) continue;
		
		uart_putc((unsigned char)digit + '0');
		anything_printed = true;
	}
	
	if (!anything_printed) {
		uart_putc('0');
	}
}

void uart_putline(){
	uart_putc('\r');
	uart_putc('\n');
}

void uart_hexdump(uintptr_t start, size_t length){
	uint32_t end = start + length;
	while (true){
		uart_puthex(start);
		
		for (uint32_t i = 0; i < 16; i++){
			uart_putc(' ');
			
			uint32_t byte = *((uint8_t*)start);
			uart_put_hexnibble(byte >> 4);
			uart_put_hexnibble(byte & 0xf);
			
			start++;
			if (start >= end){
				uart_putline();
				return;
			}
		}
		
		uart_putline();
	}
}

