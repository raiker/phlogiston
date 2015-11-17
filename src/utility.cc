#include "utility.h"
#include "panic.h"

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

//needed to permit pure virtual functions
extern "C" void __cxa_pure_virtual(){
	panic(PanicCodes::PureVirtualFunctionCall);
}

void memcpy(void * dest, void * src, size_t count){
	if ((uintptr_t)count & 0x3 || (uintptr_t)dest & 0x3 || (uintptr_t)src & 0x3){
		uint8_t *a = (uint8_t*)dest;
		uint8_t *b = (uint8_t*)src;
		
		for (size_t i = 0; i < count; i++){
			*(a++) = *(b++);
		}
	} else {
		uint32_t *a = (uint32_t*)dest;
		uint32_t *b = (uint32_t*)src;
		
		for (size_t i = 0; i < count; i+=4){
			*(a++) = *(b++);
		}
	}
}

/*void operator delete(void* ptr){
	panic(PanicCodes::AssertionFailure);
}

void operator delete(void* ptr, unsigned int size){
	panic(PanicCodes::AssertionFailure);
}*/

