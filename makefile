
# If there is a test for a component 'foo' in src/foo_test.cc, list 'foo' here
# to add that test case to the default build.
TESTS := \
	circuit \
	char_stream \
	token_stream \
	truth_table \
	truth_table_component \
	adder

OBJECTS :=  \
	build/adder.o \
	build/value.o \
	build/circuit.o \
	build/char_stream.o \
	build/location.o \
	build/parse_exception.o \
	build/token_stream.o \
	build/token_type.o \
	build/truth_table.o \
	build/truth_table_component.o

all: $(TESTS:%=build/%_test.passed)

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
