#pragma once

#include "common.h"

enum PanicCodes {
	OutOfMemory,
	InvalidParameter, //parameter invalid given current state
	IncompatibleParameter, //parameters could never be valid
	AddRefToUnallocatedPage, //add a reference to an unallocated page of memory
	ReleaseUnallocatedPage,
	TooManyReferences,
	NoMemory,
	NonZeroBase,
	AssertionFailure,
	PureVirtualFunctionCall,
};

void panic(PanicCodes code) __attribute__((noreturn));

