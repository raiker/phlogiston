CXXFLAGS="-fno-exceptions -fno-unwind-tables -fno-rtti -g -std=c++14 -Wall -Wextra" #-O3

g++ $CXXFLAGS -c testmain.cc -o build/testmain.o
g++ $CXXFLAGS -c pagetable_tests.cc -o build/pagetable_tests.o

g++ -g -o phlogiston_tests build/testmain.o build/pagetable_tests.o -lgtest
