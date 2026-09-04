.PHONY: help configure build run bandwidth test test-one test-label list-tests \
	test-system test-conformance test-performance

JOBS ?= 1
TEST ?=
LABEL ?=
CONDA_ENV ?= ../../.conda-env
CMAKE := $(CONDA_ENV)/bin/cmake
CTEST := $(CONDA_ENV)/bin/ctest

help:
	@echo "OpenHBX commands:"
	@echo "  make build                         Configure and build in the Conda environment"
	@echo "  make run                           Run the bundled config and trace"
	@echo "  make bandwidth                     Run the built maximum HBF bandwidth test"
	@echo "  make test                          Run all tests"
	@echo "  make test-one TEST=<name>          Run one exact CTest"
	@echo "  make test-label LABEL=<label>      Run a CTest label"
	@echo "  make test-system                   Run system tests"
	@echo "  make test-conformance              Run conformance tests"
	@echo "  make test-performance              Run performance tests"
	@echo "  make list-tests                    List registered tests"

configure:
	@test -x "$(CMAKE)" || \
		(echo "Conda CMake not found at $(CMAKE)"; exit 2)
	$(CMAKE) --fresh -S . -B build/tests \
		-DOPENHBF_BUILD_TESTS=ON \
		-DOPENHBF_WITH_RAMULATOR2=OFF \
		-DCMAKE_BUILD_TYPE=Debug

build: configure
	$(CMAKE) --build build/tests -j$(JOBS)

run: build
	./build/tests/openhbx_trace_runner \
		configs/products/ocp_hbf_0_7.yaml tests/traces/write_read.trace

bandwidth:
	@test -x build/tests/tests/performance/openhbx_hbf_max_read_bandwidth_test || \
		(echo "OpenHBF is not built; run 'make build' once first."; exit 2)
	$(CTEST) --test-dir build/tests \
		-R '^openhbx_hbf_max_read_bandwidth_test$$' --verbose

test: build
	$(CTEST) --test-dir build/tests --output-on-failure

test-one: build
	@test -n "$(TEST)" || (echo "usage: make test-one TEST=<exact-test-name>"; exit 2)
	$(CTEST) --test-dir build/tests \
		-R '^$(TEST)$$' --verbose --output-on-failure

test-label: build
	@test -n "$(LABEL)" || (echo "usage: make test-label LABEL=<ctest-label>"; exit 2)
	$(CTEST) --test-dir build/tests \
		-L '$(LABEL)' --output-on-failure

list-tests: configure
	$(CTEST) --test-dir build/tests -N

test-system:
	$(MAKE) test-label LABEL=openhbx_system

test-conformance:
	$(MAKE) test-label LABEL=openhbx_conformance

test-performance:
	$(MAKE) test-label LABEL=openhbx_performance
