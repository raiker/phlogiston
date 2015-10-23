#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

const size_t SECTION_SIZE = 0x100000;
const size_t PAGE_SIZE = 0x1000;

const uint32_t PAGES_IN_SECTION = SECTION_SIZE / PAGE_SIZE;

struct MemRange {
	uintptr_t start, size;
};

template<class T>
class Result {
protected:
	Result() :
		success(false)
	{ }
	
	Result(T val) :
		success(true), value(val)
	{ }
	
public:
	bool success;
	T value;
	
	static Result success(T val) {
		return Result(val);
	}
	
	static Result failure() {
		return Result();
	}
}


