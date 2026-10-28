CXX ?= g++
CXXFLAGS = -std=c++20 -O0 -g -DJSON_DIAGNOSTICS=1 -Isrc -Iinclude -Ivendor
SRC = platform database store archive zip demo bundle
OBJ = $(addprefix build/,$(addsuffix .o,$(SRC)))
all: build/core-test
build/%.o: src/%.cpp src/core.hpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -c $< -o $@
build/core-test: $(OBJ) tests/core_test.cpp
	$(CXX) $(CXXFLAGS) tests/core_test.cpp $(OBJ) -ldl -o $@
build/atlas-cli: $(OBJ) src/cli.cpp
	$(CXX) $(CXXFLAGS) src/cli.cpp $(OBJ) -ldl -o $@
test: build/core-test
	./build/core-test
