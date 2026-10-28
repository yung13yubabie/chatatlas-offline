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
build/path-identity-test: build/platform.o tests/path_identity_test.cpp
	$(CXX) $(CXXFLAGS) tests/path_identity_test.cpp build/platform.o -o $@
test: build/core-test build/zip-bounds-test build/database-contract-test build/path-identity-test
	./build/core-test
	./build/zip-bounds-test
	$(MAKE) test-database
	$(MAKE) test-path-identity
.PHONY: test test-database test-path-identity
test-database: build/database-contract-test
	@d=$$(mktemp -d) || exit 1; ./build/database-contract-test "$$d"; status=$$?; rm -rf "$$d"; exit $$status

test-path-identity: build/path-identity-test
	@d=$$(mktemp -d) || exit 1; ./build/path-identity-test "$$d"; status=$$?; rm -rf "$$d"; exit $$status

build/reader-fixture: $(OBJ) tests/reader_fixture.cpp
	$(CXX) $(CXXFLAGS) tests/reader_fixture.cpp $(OBJ) -ldl -o $@
