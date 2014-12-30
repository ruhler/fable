
all: build/circuit_test.passed

build/circuit_test.passed: build/circuit_test
	./build/circuit_test && echo "PASSED" > $@

build/circuit_test: circuit/circuit_test.cc
	mkdir -p build
	g++ -o $@ $< -lgtest -lgtest_main -lpthread

