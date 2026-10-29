CXX ?= g++
CXXFLAGS = -std=c++20 -O0 -g -DJSON_DIAGNOSTICS=1 -Isrc -Iinclude -Ivendor -I../.toolchains/sqlite3mc-2.6.1
SRC = platform database store archive zip demo bundle import line_cipher line_process line_media line_database line_obs line_locations line_batch text_export
OBJ = $(addprefix build/,$(addsuffix .o,$(SRC))) build/line_cipher_engine.o
all: build/core-test
build/%.o: src/%.cpp $(wildcard src/*.hpp) | build/line_cipher_engine.o
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -c $< -o $@
build/line_cipher_engine.o: ../tools/fetch_line_cipher.mjs ../tools/build_line_cipher.mjs
	@mkdir -p build
	node ../tools/build_line_cipher.mjs --cc '$(CC)' --output $@
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
test: build/time-contract-test build/private-path-contract-test build/blob-path-contract-test build/blob-path-store-test build/item-value-contract-test build/store-cancel-test build/request-control-test build/bridge-timing-test build/core-test build/zip-bounds-test build/database-contract-test build/path-identity-test build/capture-history-test build/import-test
	./build/time-contract-test
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

build/line-cipher-test: $(OBJ) tests/line_cipher_test.cpp
	$(CXX) $(CXXFLAGS) tests/line_cipher_test.cpp $(OBJ) -ldl -pthread -o $@
build/line-process-test: $(OBJ) tests/line_process_test.cpp
	$(CXX) $(CXXFLAGS) tests/line_process_test.cpp $(OBJ) -ldl -pthread -o $@
build/line-media-test: $(OBJ) tests/line_media_test.cpp
	$(CXX) $(CXXFLAGS) tests/line_media_test.cpp $(OBJ) -ldl -pthread -o $@
build/line-database-test: $(OBJ) tests/line_database_test.cpp
	$(CXX) $(CXXFLAGS) tests/line_database_test.cpp $(OBJ) -ldl -pthread -o $@
.PHONY: test-line-cipher test-line-process test-line-media test-line-database
test: test-line-cipher test-line-process test-line-media test-line-database
test-line-process: build/line-process-test
	./build/line-process-test
test-line-cipher: build/line-cipher-test
	@d=$$(mktemp -d) || exit 1; ./build/line-cipher-test "$$d"; status=$$?; rm -rf "$$d"; exit $$status
test-line-media: build/line-media-test
	@d=$$(mktemp -d) || exit 1; ./build/line-media-test "$$d"; status=$$?; rm -rf "$$d"; exit $$status
test-line-database: build/line-database-test
	@d=$$(mktemp -d) || exit 1; ./build/line-database-test "$$d"; status=$$?; rm -rf "$$d"; exit $$status

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

build/time-contract-test: $(OBJ) tests/time_contract_test.cpp
	$(CXX) $(CXXFLAGS) tests/time_contract_test.cpp $(OBJ) -ldl -o $@

build/line-obs-test: $(OBJ) tests/line_obs_test.cpp
	$(CXX) $(CXXFLAGS) tests/line_obs_test.cpp $(OBJ) -ldl -pthread -o $@
.PHONY: test-line-obs
test: test-line-obs
test-line-obs: build/line-obs-test
	./build/line-obs-test

build/line-locations-test: build/platform.o build/line_locations.o tests/line_locations_test.cpp
	$(CXX) $(CXXFLAGS) tests/line_locations_test.cpp build/platform.o build/line_locations.o -o $@
.PHONY: test-line-locations
test: test-line-locations
test-line-locations: build/line-locations-test
	@d=$$(mktemp -d) || exit 1; ./build/line-locations-test "$$d"; status=$$?; rm -rf "$$d"; exit $$status

build/line-batch-test: $(OBJ) tests/line_batch_test.cpp
	$(CXX) $(CXXFLAGS) tests/line_batch_test.cpp $(OBJ) -ldl -pthread -o $@
.PHONY: test-line-batch
test: test-line-batch
test-line-batch: build/line-batch-test
	@d=$$(mktemp -d) || exit 1; ./build/line-batch-test "$$d"; status=$$?; rm -rf "$$d"; exit $$status

build/text-export-test: $(OBJ) tests/text_export_test.cpp
	$(CXX) $(CXXFLAGS) tests/text_export_test.cpp $(OBJ) -ldl -pthread -o $@
.PHONY: test-text-export
test: test-text-export
test-text-export: build/text-export-test
	@d=$$(mktemp -d) || exit 1; ./build/text-export-test "$$d"; status=$$?; rm -rf "$$d"; exit $$status

build/name-retention-test: $(OBJ) tests/name_retention_test.cpp
	$(CXX) $(CXXFLAGS) tests/name_retention_test.cpp $(OBJ) -ldl -o $@
.PHONY: test-name-retention
test: test-name-retention
test-name-retention: build/name-retention-test
	@d=$$(mktemp -d) || exit 1; ./build/name-retention-test "$$d"; status=$$?; rm -rf "$$d"; exit $$status
