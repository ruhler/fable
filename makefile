
default: tests

# foo_HDEPS: A list of files that anything including foo.h depends on due to
# it including foo.h.
adder_HDEPS := src/adder.h
location_HDEPS := src/location.h
char_stream_HDEPS := src/char_stream.h $(location_HDEPS)
value_HDEPS := src/value.h
circuit_HDEPS := src/circuit.h $(value_HDEPS)
token_type_HDEPS := src/token_type.h
parse_exception_HDEPS := src/parse_exception.h $(location_HDEPS) $(token_type_HDEPS)
token_stream_HDEPS := src/token_stream.h $(char_stream_HDEPS) $(token_type_HDEPS)
truth_table_HDEPS := src/truth_table.h
truth_table_component_HDEPS := src/truth_table_component.h $(circuit_HDEPS) $(truth_table_HDEPS) $(value_HDEPS)

# foo_CDEPS: A list of the header files foo.cc depends on.
adder_CDEPS := $(circuit_HDEPS) $(truth_table_HDEPS) $(truth_table_component_HDEPS)
adder_test_CDEPS := $(adder_HDEPS) $(circuit_HDEPS)
char_stream_CDEPS := $(char_stream_HDEPS)
char_stream_test_CDEPS := $(char_stream_HDEPS)
circuit_CDEPS := $(circuit_HDEPS)
circuit_test_CDEPS := $(circuit_HDEPS) $(value_HDEPS)
location_CDEPS := $(location_HDEPS)
parse_exception_CDEPS := $(parse_exception_HDEPS)
token_stream_CDEPS := $(token_stream_HDEPS) $(parse_exception_HDEPS)
token_stream_test_CDEPS := $(char_stream_HDEPS) $(parse_exception_HDEPS) $(token_stream_HDEPS)
token_type_CDEPS := $(token_type_HDEPS)
truth_table_CDEPS := $(truth_table_HDEPS)
truth_table_component_CDEPS := $(truth_table_component_HDEPS)
truth_table_component_test_CDEPS := $(truth_table_component_HDEPS)
truth_table_test_CDEPS := $(truth_table_HDEPS)
value_CDEPS := $(value_HDEPS)

# foo_ODEPS: A list of object files an executable depends on.
adder_test_ODEPS := build/adder_test.o build/adder.o build/circuit.o build/value.o build/truth_table.o build/truth_table_component.o
char_stream_test_ODEPS := build/char_stream_test.o build/char_stream.o build/location.o
circuit_test_ODEPS := build/circuit_test.o build/circuit.o build/value.o
token_stream_test_ODEPS := build/token_stream_test.o build/char_stream.o build/parse_exception.o build/location.o build/token_stream.o build/token_type.o
truth_table_component_test_ODEPS := build/truth_table_component_test.o build/truth_table_component.o build/truth_table.o build/circuit.o build/value.o
truth_table_test_ODEPS := build/truth_table_test.o build/truth_table.o

build/adder.o: src/adder.cc $(adder_CDEPS)
	mkdir -p build
	g++ -ggdb -std=c++11 -c -o $@ $<

build/adder_test.o: src/adder_test.cc $(adder_test_CDEPS)
	mkdir -p build
	g++ -ggdb -std=c++11 -c -o $@ $<

build/char_stream.o: src/char_stream.cc $(char_stream_CDEPS)
	mkdir -p build
	g++ -ggdb -std=c++11 -c -o $@ $<

build/char_stream_test.o: src/char_stream_test.cc $(char_stream_test_CDEPS)
	mkdir -p build
	g++ -ggdb -std=c++11 -c -o $@ $<

build/circuit.o: src/circuit.cc $(circuit_CDEPS)
	mkdir -p build
	g++ -ggdb -std=c++11 -c -o $@ $<

build/circuit_test.o: src/circuit_test.cc $(circuit_test_CDEPS)
	mkdir -p build
	g++ -ggdb -std=c++11 -c -o $@ $<

build/location.o: src/location.cc $(location_CDEPS)
	mkdir -p build
	g++ -ggdb -std=c++11 -c -o $@ $<

build/parse_exception.o: src/parse_exception.cc $(parse_exception_CDEPS)
	mkdir -p build
	g++ -ggdb -std=c++11 -c -o $@ $<

build/token_stream.o: src/token_stream.cc $(token_stream_CDEPS)
	mkdir -p build
	g++ -ggdb -std=c++11 -c -o $@ $<

build/token_stream_test.o: src/token_stream_test.cc $(token_stream_test_CDEPS)
	mkdir -p build
	g++ -ggdb -std=c++11 -c -o $@ $<

build/token_type.o: src/token_type.cc $(token_type_CDEPS)
	mkdir -p build
	g++ -ggdb -std=c++11 -c -o $@ $<

build/truth_table.o: src/truth_table.cc $(truth_table_CDEPS)
	mkdir -p build
	g++ -ggdb -std=c++11 -c -o $@ $<
	
build/truth_table_component.o: src/truth_table_component.cc $(truth_table_component_CDEPS)
	mkdir -p build
	g++ -ggdb -std=c++11 -c -o $@ $<

build/truth_table_component_test.o: src/truth_table_component_test.cc $(truth_table_component_test_CDEPS)
	mkdir -p build
	g++ -ggdb -std=c++11 -c -o $@ $<

build/truth_table_test.o: src/truth_table_test.cc $(truth_table_test_CDEPS)
	mkdir -p build
	g++ -ggdb -std=c++11 -c -o $@ $<

build/value.o: src/value.cc $(value_CDEPS)
	mkdir -p build
	g++ -ggdb -std=c++11 -c -o $@ $<

build/adder_test: $(adder_test_ODEPS)
	mkdir -p build
	g++ -ggdb -std=c++11 -o $@ $^ -lgtest -lgtest_main -lpthread

build/char_stream_test: $(char_stream_test_ODEPS)
	mkdir -p build
	g++ -ggdb -std=c++11 -o $@ $^ -lgtest -lgtest_main -lpthread

build/circuit_test: $(circuit_test_ODEPS)
	mkdir -p build
	g++ -ggdb -std=c++11 -o $@ $^ -lgtest -lgtest_main -lpthread

build/token_stream_test: $(token_stream_test_ODEPS)
	mkdir -p build
	g++ -ggdb -std=c++11 -o $@ $^ -lgtest -lgtest_main -lpthread

build/truth_table_component_test: $(truth_table_component_test_ODEPS)
	mkdir -p build
	g++ -ggdb -std=c++11 -o $@ $^ -lgtest -lgtest_main -lpthread

build/truth_table_test: $(truth_table_test_ODEPS)
	mkdir -p build
	g++ -ggdb -std=c++11 -o $@ $^ -lgtest -lgtest_main -lpthread

# If there is a test for a component 'foo' in src/foo_test.cc, list 'foo' here
# to add that test case to the default build.
TESTS := \
	circuit \
	char_stream \
	token_stream \
	truth_table \
	truth_table_component \
	adder

tests: $(TESTS:%=build/%_test.passed)

build/%_test.passed: build/%_test
	./build/$*_test && echo "PASSED" > $@

.PHONY: clean
clean: 
	rm -rf build
