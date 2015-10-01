#include "utility.h"

/* Loop <delay> times in a way that the compiler won't optimize away. */
void delay(int32_t count){
	asm volatile("__delay_%=: subs %[count], %[count], #1; bne __delay_%=\n"
		 : : [count]"r"(count) : "cc");
}

size_t strlen(const char* str){
	size_t ret = 0;
	while ( str[ret] != 0 )
		ret++;
	return ret;
}


