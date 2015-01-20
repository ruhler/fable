
# declare-header.
# (1) - The name of the header to declare.
#       It's assumed to be in 'src/' directory with extension .h, so don't
#       include the src/ directory or the .h extension.
# (2) - The name of each header this header directly depends on.
define declare-header
  $(1)_HDEPS := src/$(1).h $(2:%=$$(%_HDEPS))
endef

$(eval $(call declare-header,adder,))
$(eval $(call declare-header,location,))
$(eval $(call declare-header,char_stream,location))
$(eval $(call declare-header,value,))
$(eval $(call declare-header,circuit,value))
$(eval $(call declare-header,token_type,))
$(eval $(call declare-header,parse_exception,location token_type))
$(eval $(call declare-header,token_stream,char_stream token_type))
$(eval $(call declare-header,truth_table,))
$(eval $(call declare-header,truth_table_component,circuit truth_table value))

default: tests


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


build/all_test: $(TESTS:build/%_test=build/%_test.o) $(adder_test_ODEPS) $(char_stream_test_ODEPS) $(circuit_test_ODEPS) $(token_stream_test_ODEPS) $(truth_table_component_test_ODEPS) $(truth_table_test_ODEPS)
	mkdir -p build
	g++ -ggdb -std=c++11 -o $@ $^ -lgtest -lgtest_main -lpthread

tests: build/all_test.passed

build/all_test.passed: build/all_test
	./build/all_test && echo "PASSED" > $@

.PHONY: clean
clean: 
	rm -rf build
