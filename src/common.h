#pragma once

#include <cstdbool>
#include <cstddef>
#include <cstdint>

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

template<class E>
class ErrorResult {
protected:
	bool m_is_error;
	E m_error;
	
	ErrorResult() :
		m_is_error(false)
	{ }
	
	ErrorResult(E err) :
		m_is_error(true),
		m_error(err)
	{ }
public:
	bool is_error(){
		return m_is_error;
	}
	
	E & get_error(){
		return m_error;
	}
	
	static ErrorResult success() {
		return ErrorResult();
	}
	
	static ErrorResult failure(E err) {
		return ErrorResult(err);
	}
}

template<class T, class E>
class Result {
protected:
	bool m_is_error;
	union {
		T m_value,
		E m_error
	};
	
	Result(E err) :
		m_is_error(true),
		m_error(err)
	{ }
	
	Result(T val) :
		m_is_error(false),
		m_value(val)
	{ }
	
public:
	bool is_success(){
		return !m_is_error;
	}
	
	bool is_error(){
		return m_is_error;
	}
	
	T & get_value(){
		return m_value;
	}
	
	E & get_error(){
		return m_error;
	}
	static Result success(T val) {
		return Result(val);
	}
	
	static Result failure(E err) {
		return Result(err);
	}
};

