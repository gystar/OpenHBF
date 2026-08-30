CXX ?= c++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Werror -O2
ROOT := ../..
INCLUDES := -I$(ROOT)/include
ENGINE := $(ROOT)/src/integration/integration_engine.cpp

.PHONY: all test clean runner
all: test_integration openhbf_trace_runner

test_integration: test_integration.cpp $(ENGINE)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@

openhbf_trace_runner: $(ROOT)/apps/openhbf_trace_runner.cpp $(ENGINE)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@

test: all
	./test_integration
	./openhbf_trace_runner $(ROOT)/tests/traces/write_read.trace $(ROOT)/configs/openhbf-default.yaml >/dev/null

clean:
	rm -f test_integration openhbf_trace_runner
