
OBJECTS := build/zero.o build/one.o build/value.o build/circuit.o build/nand.o

all: build/circuit_test.passed

build/circuit_test.passed: build/circuit_test
	./build/circuit_test && echo "PASSED" > $@

build/circuit_test: src/circuit_test.cc $(OBJECTS)
	mkdir -p build
	g++ -o $@ $< $(OBJECTS) -lgtest -lgtest_main -lpthread

build/%.o: src/%.cc src/%.h
	mkdir -p build
	g++ -c -o $@ $<

build/zero.o: src/value.h src/circuit.h
build/one.o: src/value.h src/circuit.h
build/nand.o: src/value.h src/circuit.h
build/circuit.o: src/value.h

.PHONY: clean
clean: 
	rm -rf build
