
default: build/tests_passed

# declare-header.
# (1) - The name of the header to declare.
#       It's assumed to be in 'src/' directory with extension .h, so don't
#       include the src/ directory or the .h extension.
# (2) - The name of each header this header directly depends on.
#
# Defines foo_HDEPS, a list of all header files this header depends on,
# including itself, direct dependencies, and all indirect dependencies.
#
# Also defines a rule to check the dependencies in the header, adding the
# target file to HEADER_DEP_CHECKS
define declare-header
$(1)_HDEPS := src/$(1).h $(2:%=$$(%_HDEPS))
build/$(1).h.deps_right: src/$(1).h
	python util/checkdep.py src/$(1).h $(2:%=%.h) && touch $$@
HEADER_DEP_CHECKS += build/$(1).h.deps_right
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
$(eval $(call declare-header,truth_table_parser,truth_table))


# declare-impl.
# (1) - The name of the implementation to declare.
#       It's assumed to be in 'src/' directory with extension .cc, so don't
#       include the src/ directory or the .cc extension.
# (2) - The name of each header this implementation directly depends on.
#
# Also defines a rule to check the dependencies in the implementation file,
# adding the target file to IMPL_DEP_CHECKS
define declare-impl
ALL_OBJECTS += build/$(1).o
build/$(1).o: src/$(1).cc $(2:%=$$(%_HDEPS))
	mkdir -p build
	g++ -ggdb -std=c++11 -c -o $$@ $$<
build/$(1).cc.deps_right: src/$(1).cc
	python util/checkdep.py src/$(1).cc $(2:%=%.h) && touch $$@
IMPL_DEP_CHECKS += build/$(1).cc.deps_right
endef

$(eval $(call declare-impl,adder,adder circuit truth_table truth_table_component))
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
$(eval $(call declare-impl,truth_table_parser,truth_table_parser token_stream parse_exception))
$(eval $(call declare-impl,truth_table_parser_test,truth_table truth_table_parser parse_exception))
$(eval $(call declare-impl,truth_table_test,truth_table))
$(eval $(call declare-impl,value,value))

build/run_tests: $(ALL_OBJECTS)
	mkdir -p build
	g++ -ggdb -std=c++11 -o $@ $^ -lgtest -lgtest_main -lpthread

build/tests_passed: build/run_tests $(HEADER_DEP_CHECKS) $(IMPL_DEP_CHECKS)
	./build/run_tests && touch $@

.PHONY: clean
clean: 
	rm -rf build

