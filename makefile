
OBJECTS :=  \
	build/adder.o \
	build/value.o \
	build/circuit.o \
	build/truth_table.o \
	build/truth_table_component.o

all: \
	build/circuit_test.passed \
 	build/truth_table_test.passed \
	build/truth_table_component_test.passed \
	build/adder_test.passed

build/%_test.passed: build/%_test
	./build/$*_test && echo "PASSED" > $@

build/%_test: src/%_test.cc $(OBJECTS)
	mkdir -p build
	g++ -ggdb -std=c++11 -o $@ $< $(OBJECTS) -lgtest -lgtest_main -lpthread

build/%.o: src/%.cc src/%.h
	mkdir -p build
	g++ -ggdb -std=c++11 -c -o $@ $<

build/circuit.o: src/value.h

.PHONY: clean
clean: 
	rm -rf build
