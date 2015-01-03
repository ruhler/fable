
OBJECTS := build/zero.o \
	build/one.o \
	build/value.o \
	build/circuit.o \
	build/nand.o \
	build/truth_table.o

all: build/circuit_test.passed build/truth_table_test.passed

build/%_test.passed: build/%_test
	./build/$*_test && echo "PASSED" > $@

build/%_test: src/%_test.cc $(OBJECTS)
	mkdir -p build
	g++ -std=c++11 -o $@ $< $(OBJECTS) -lgtest -lgtest_main -lpthread

build/%.o: src/%.cc src/%.h
	mkdir -p build
	g++ -std=c++11 -c -o $@ $<

build/zero.o: src/value.h src/circuit.h
build/one.o: src/value.h src/circuit.h
build/nand.o: src/value.h src/circuit.h
build/circuit.o: src/value.h

.PHONY: clean
clean: 
	rm -rf build
