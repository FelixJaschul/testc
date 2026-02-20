all: build run
allc: clean build run

build:
	mkdir -p cmake-build-debug
	cd cmake-build-debug && cmake .. -DCMAKE_EXPORT_COMPILE_COMMANDS=ON && make -j$(shell sysctl -n hw.logicalcpu)

run:
	cmake-build-debug/implc

clean:
	rm -rf cmake-build-debug
