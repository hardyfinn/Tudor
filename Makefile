# Tudor for VCV Rack.
# Build:  export RACK_DIR=/path/to/Rack-SDK && make && make install   (or make dist)
# Tests:  make test   (host build of the DSP models, no Rack SDK needed)

RACK_DIR ?= ../..

# No -Isrc: our src/dsp headers must not shadow Rack's <dsp/...> on case-insensitive filesystems.
CXXFLAGS += -Wall -Wextra -Wno-unused-parameter $(EXTRA_CXXFLAGS)

SOURCES += $(wildcard src/*.cpp src/modules/*.cpp)

DISTRIBUTABLES += res
DISTRIBUTABLES += $(wildcard LICENSE*)
DISTRIBUTABLES += README.md

ifneq ($(wildcard $(RACK_DIR)/plugin.mk),)
include $(RACK_DIR)/plugin.mk
endif

TEST_CXX ?= g++
build/tudor_tests: $(wildcard tests/*.cpp tests/*.hpp src/dsp/*.hpp)
	@mkdir -p build
	$(TEST_CXX) -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -o $@ $(wildcard tests/*.cpp)

.PHONY: test panels
test: build/tudor_tests
	./build/tudor_tests

# Regenerate res/*.svg and src/generated/Layout.hpp (needs Python 3 and fonttools).
panels:
	python3 scripts/make_panels.py
