CXX ?= g++
CXXFLAGS = -std=c++20 -O0 -g -DJSON_DIAGNOSTICS=1 -Isrc -Iinclude -Ivendor
SRC = platform database store archive zip demo bundle
OBJ = $(addprefix build/,$(addsuffix .o,$(SRC)))
all: build/core-test
build/%.o: src/%.cpp $(wildcard src/*.hpp)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -c $< -o $@
build/core-test: $(OBJ) tests/core_test.cpp
	$(CXX) $(CXXFLAGS) tests/core_test.cpp $(OBJ) -ldl -o $@
build/atlas-cli: $(OBJ) src/cli.cpp
	$(CXX) $(CXXFLAGS) src/cli.cpp $(OBJ) -ldl -o $@
build/zip-bounds-test: $(filter-out build/zip.o,$(OBJ)) tests/zip_bounds_test.cpp src/zip.cpp src/zip_bounds.hpp
	$(CXX) $(CXXFLAGS) tests/zip_bounds_test.cpp $(filter-out build/zip.o,$(OBJ)) -ldl -o $@
build/database-contract-test: build/database.o build/platform.o tests/database_contract_test.cpp src/database_bounds.hpp
	$(CXX) $(CXXFLAGS) tests/database_contract_test.cpp build/database.o build/platform.o -ldl -o $@
test: build/core-test build/zip-bounds-test build/database-contract-test
	./build/core-test
	./build/zip-bounds-test
	@d=$(mktemp -d); ./build/database-contract-test "$d"; status=$?; rm -rf "$d"; exit $status
