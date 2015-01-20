
default: tests

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


# declare-impl.
# (1) - The name of the implementation to declare.
#       It's assumed to be in 'src/' directory with extension .cc, so don't
#       include the src/ directory or the .cc extension.
# (2) - The name of each header this implementation directly depends on.
define declare-impl
build/$(1).o: src/$(1).cc $(2:%=$$(%_HDEPS))
	mkdir -p build
	g++ -ggdb -std=c++11 -c -o $$@ $$<
endef

$(eval $(call declare-impl,adder,circuit truth_table truth_table_component))
$(eval $(call declare-impl,adder_test,adder circuit))
$(eval $(call declare-impl,char_stream,char_stream))
$(eval $(call declare-impl,char_stream_test,char_stream))
$(eval $(call declare-impl,circuit,circuit))
$(eval $(call declare-impl,circuit_test,circuit value))
$(eval $(call declare-impl,location,location))
$(eval $(call declare-impl,parse_exception,parse_exception))
$(eval $(call declare-impl,token_stream,token_stream parse_exception))
$(eval $(call declare-impl,token_stream_test,char_stream parse_exception token_stream))
$(eval $(call declare-impl,token_type,token_type))
$(eval $(call declare-impl,truth_table,truth_table))
$(eval $(call declare-impl,truth_table_component,truth_table_component))
$(eval $(call declare-impl,truth_table_component_test,truth_table_component))
$(eval $(call declare-impl,truth_table_test,truth_table))
$(eval $(call declare-impl,value,value))

# foo_ODEPS: A list of object files an executable depends on.
adder_test_ODEPS := build/adder_test.o build/adder.o build/circuit.o build/value.o build/truth_table.o build/truth_table_component.o
char_stream_test_ODEPS := build/char_stream_test.o build/char_stream.o build/location.o
circuit_test_ODEPS := build/circuit_test.o build/circuit.o build/value.o
token_stream_test_ODEPS := build/token_stream_test.o build/char_stream.o build/parse_exception.o build/location.o build/token_stream.o build/token_type.o
truth_table_component_test_ODEPS := build/truth_table_component_test.o build/truth_table_component.o build/truth_table.o build/circuit.o build/value.o
truth_table_test_ODEPS := build/truth_table_test.o build/truth_table.o


build/all_test: $(TESTS:build/%_test=build/%_test.o) $(adder_test_ODEPS) $(char_stream_test_ODEPS) $(circuit_test_ODEPS) $(token_stream_test_ODEPS) $(truth_table_component_test_ODEPS) $(truth_table_test_ODEPS)
	mkdir -p build
	g++ -ggdb -std=c++11 -o $@ $^ -lgtest -lgtest_main -lpthread

tests: build/all_test.passed

build/all_test.passed: build/all_test
	./build/all_test && echo "PASSED" > $@

.PHONY: clean
clean: 
	rm -rf build
