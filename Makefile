.PHONY : build clean format test-cpp

TYPE ?= Release
BACKTRACE ?= ON
TEST ?= ON
FORMAT_ORIGIN ?=

CMAKE_OPT = -DCMAKE_BUILD_TYPE=$(TYPE)
CMAKE_OPT += -DUSE_BACKTRACE=$(BACKTRACE)
CMAKE_OPT += -DBUILD_TEST=$(TEST)

build:
	mkdir -p build/$(TYPE)
	cd build/$(TYPE) && cmake $(CMAKE_OPT) ../.. && make -j8

clean:
	rm -rf build

format:
	@python3 scripts/format.py $(FORMAT_ORIGIN)

test-cpp:
	@echo
	cd build/$(TYPE) && make test