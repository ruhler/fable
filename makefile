
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

# declare-test.
# (1) - The name of the test to declare.
#       It's assumed to be in 'src/' directory with suffix _test.cc, so don't
#       include the src/ directory or the _test.cc extension.
# (2) - The name of each impl this test directly or indirectly depends on.
define declare-test
	TEST_DEPS += build/$(1)_test.o $(2:%=build/%.o)
endef

$(eval $(call declare-test,adder,adder circuit value truth_table truth_table_component))
$(eval $(call declare-test,char_stream,char_stream location))
$(eval $(call declare-test,circuit,circuit value))
$(eval $(call declare-test,token_stream,token_stream char_stream parse_exception location token_type))
$(eval $(call declare-test,truth_table_component,truth_table_component truth_table circuit value))
$(eval $(call declare-test,truth_table,truth_table))


build/run_tests: $(TEST_DEPS)
	mkdir -p build
	g++ -ggdb -std=c++11 -o $@ $^ -lgtest -lgtest_main -lpthread

tests: build/run_tests.passed

build/run_tests.passed: build/run_tests
	./build/run_tests && echo "PASSED" > $@

.PHONY: clean
clean: 
	rm -rf build

