
OBJECTS := build/zero.o build/one.o build/value.o build/circuit.o

all: build/circuit_test.passed

build/circuit_test.passed: build/circuit_test
	./build/circuit_test && echo "PASSED" > $@

build/circuit_test: circuit/circuit_test.cc $(OBJECTS)
	mkdir -p build
	g++ -o $@ $< $(OBJECTS) -lgtest -lgtest_main -lpthread

build/%.o: circuit/%.cc circuit/%.h
	g++ -c -o $@ $<

build/zero.o: circuit/value.h circuit/circuit.h
build/one.o: circuit/value.h circuit/circuit.h
build/circuit.o: circuit/value.h

