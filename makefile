
.PHONY: all
all:
	tclsh build.tcl

.PHONY: clean
clean:
	rm -rf out/

.PHONY: checkerr
checkerr: all
	cat out/test/*.err

.PHONY: foo
foo:
	#./out/bin/fble-test prgms/fble-tictactoe.fble prgms
	#./out/bin/fble-tests prgms/fble-tests.fble prgms
	./out/bin/fble-tests prgms/fble-tictactoe-ai.fble prgms

.PHONY: perf
perf:
	rm -f gmon.out out/fble/obj/*.gcda
	./out/fble/fble-test prgms/AllTests.fble prgms
	mkdir -p perf
	gprof out/fble/fble-test > perf/gprof.txt
	gcov out/fble/obj/eval.o out/fble/obj/compile.o out/fble/obj/ref.o out/fble/obj/value.o> perf/fble.gcov
	mv *.gcov perf/
	rm gmon.out

