#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

const size_t SUPERSECTION_SIZE = 0x1000000;
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
		is_success(false)
	{ }
	
	Result(T val) :
		is_success(true), value(val)
	{ }
	
public:
	bool is_success;
	T value;
	
	static Result success(T val) {
		return Result(val);
	}
	
	static Result failure() {
		return Result();
	}
};


