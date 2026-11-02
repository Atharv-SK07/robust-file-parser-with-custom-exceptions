# Optional: only needed for g++ users. Visual Studio / Code::Blocks do not use this file.
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -pedantic -O1
LIB_SRC  := $(filter-out src/main.cpp,$(wildcard src/*.cpp))

all: rfp_demo rfp_tests

rfp_demo: $(wildcard src/*.cpp) $(wildcard src/*.h)
	$(CXX) $(CXXFLAGS) -o $@ src/*.cpp

rfp_tests: $(LIB_SRC) tests/test_main.cpp $(wildcard src/*.h)
	$(CXX) $(CXXFLAGS) -Isrc -o $@ $(LIB_SRC) tests/test_main.cpp

test: rfp_tests
	./rfp_tests

clean:
	rm -f rfp_demo rfp_tests tmp_*

.PHONY: all test clean
