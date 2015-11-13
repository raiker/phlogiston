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

/*void operator delete(void* ptr){
	panic(PanicCodes::AssertionFailure);
}

void operator delete(void* ptr, unsigned int size){
	panic(PanicCodes::AssertionFailure);
}*/

