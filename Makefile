.PHONY: help configure build run test test-one test-label list-tests \
	test-system test-conformance test-performance

JOBS ?= 2
TEST ?=
LABEL ?=

help:
	@echo "OpenHBX commands:"
	@echo "  make build                         Configure and build in Docker"
	@echo "  make run                           Run the bundled config and trace"
	@echo "  make test                          Run all tests"
	@echo "  make test-one TEST=<name>          Run one exact CTest"
	@echo "  make test-label LABEL=<label>      Run a CTest label"
	@echo "  make test-system                   Run system tests"
	@echo "  make test-conformance              Run conformance tests"
	@echo "  make test-performance              Run performance tests"
	@echo "  make list-tests                    List registered tests"

configure:
	docker compose up -d dev
	docker compose exec -T dev cmake -S . -B build/tests \
		-DOPENHBF_BUILD_TESTS=ON \
		-DOPENHBF_WITH_RAMULATOR2=OFF \
		-DCMAKE_BUILD_TYPE=Debug

build: configure
	docker compose exec -T dev cmake --build build/tests -j$(JOBS)

run: build
	docker compose exec -T dev ./build/tests/openhbx_trace_runner \
		configs/products/ocp_hbf_0_7.yaml tests/traces/write_read.trace

test: build
	docker compose exec -T dev ctest --test-dir build/tests --output-on-failure

test-one: build
	@test -n "$(TEST)" || (echo "usage: make test-one TEST=<exact-test-name>"; exit 2)
	docker compose exec -T dev ctest --test-dir build/tests \
		-R '^$(TEST)$$' --verbose --output-on-failure

test-label: build
	@test -n "$(LABEL)" || (echo "usage: make test-label LABEL=<ctest-label>"; exit 2)
	docker compose exec -T dev ctest --test-dir build/tests \
		-L '$(LABEL)' --output-on-failure

list-tests: configure
	docker compose exec -T dev ctest --test-dir build/tests -N

test-system:
	$(MAKE) test-label LABEL=openhbx_system

test-conformance:
	$(MAKE) test-label LABEL=openhbx_conformance

test-performance:
	$(MAKE) test-label LABEL=openhbx_performance
