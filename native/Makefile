CXX ?= g++
CXXFLAGS = -std=c++20 -O0 -g -DJSON_DIAGNOSTICS=1 -Isrc -Iinclude -Ivendor
SRC = platform database store archive zip demo bundle import
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
test: build/private-path-contract-test build/blob-path-contract-test build/blob-path-store-test build/item-value-contract-test build/store-cancel-test build/request-control-test build/bridge-timing-test build/core-test build/zip-bounds-test build/database-contract-test build/path-identity-test build/capture-history-test build/import-test
	./build/private-path-contract-test
	./build/blob-path-contract-test
	$(MAKE) test-blob-path-store
	./build/request-control-test
	./build/bridge-timing-test
	./build/core-test
	./build/zip-bounds-test
	$(MAKE) test-database
	$(MAKE) test-item-values
	$(MAKE) test-path-identity
	$(MAKE) test-store-cancel
	$(MAKE) test-import
	$(MAKE) test-capture-history
.PHONY: test test-database test-path-identity
test-database: build/database-contract-test
	@d=$$(mktemp -d) || exit 1; ./build/database-contract-test "$$d"; status=$$?; rm -rf "$$d"; exit $$status

test-path-identity: build/path-identity-test
	@d=$$(mktemp -d) || exit 1; ./build/path-identity-test "$$d"; status=$$?; rm -rf "$$d"; exit $$status

build/reader-fixture: $(OBJ) tests/reader_fixture.cpp
	$(CXX) $(CXXFLAGS) tests/reader_fixture.cpp $(OBJ) -ldl -o $@

build/capture-history-test: $(OBJ) tests/capture_history_test.cpp
	$(CXX) $(CXXFLAGS) tests/capture_history_test.cpp $(OBJ) -ldl -o $@
build/import-test: $(OBJ) tests/import_test.cpp
	$(CXX) $(CXXFLAGS) tests/import_test.cpp $(OBJ) -ldl -o $@
.PHONY: test-capture-history test-import
test-capture-history: build/capture-history-test
	@d=$$(mktemp -d) || exit 1; ./build/capture-history-test "$$d"; status=$$?; rm -rf "$$d"; exit $$status
test-import: build/import-test
	@d=$$(mktemp -d) || exit 1; ./build/import-test "$$d"; status=$$?; rm -rf "$$d"; exit $$status

build/bridge-timing-test: tests/bridge_timing_test.cpp src/bridge_timing.hpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) tests/bridge_timing_test.cpp -o $@

build/request-control-test: tests/request_control_test.cpp src/request_control.hpp src/bridge_timing.hpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) tests/request_control_test.cpp -o $@

build/store-cancel-test: $(OBJ) tests/store_cancel_test.cpp
	$(CXX) $(CXXFLAGS) tests/store_cancel_test.cpp $(OBJ) -ldl -o $@
.PHONY: test-store-cancel
test-store-cancel: build/store-cancel-test
	@d=$$(mktemp -d) || exit 1; ./build/store-cancel-test "$$d"; status=$$?; rm -rf "$$d"; exit $$status

build/item-value-contract-test: $(OBJ) tests/item_value_contract_test.cpp
	$(CXX) $(CXXFLAGS) tests/item_value_contract_test.cpp $(OBJ) -ldl -o $@
.PHONY: test-item-values
test-item-values: build/item-value-contract-test
	./build/item-value-contract-test

# Storage path validation uses the same contract at registration, reuse and export.
build/blob-path-contract-test: tests/blob_path_contract_test.cpp src/blob_path.hpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) tests/blob_path_contract_test.cpp -o $@
build/blob-path-store-test: $(OBJ) tests/blob_path_store_test.cpp
	$(CXX) $(CXXFLAGS) tests/blob_path_store_test.cpp $(OBJ) -ldl -o $@
.PHONY: test-blob-path-store
test-blob-path-store: build/blob-path-store-test
	@d=$$(mktemp -d) || exit 1; ./build/blob-path-store-test "$$d"; status=$$?; rm -rf "$$d"; exit $$status

# Pure local-private-path grammar contract, without contacting a network.
build/private-path-contract-test: tests/private_path_contract_test.cpp src/private_path.hpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) tests/private_path_contract_test.cpp -o $@
