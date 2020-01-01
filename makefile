
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
	#./out/bin/fble-test prgms/stack-smasher.fble
	./out/bin/fble-stdio prgms/fble-sudoku.fble prgms/
	#./out/bin/fble-stdio prgms/fble-tictactoe.fble prgms/
	#./out/bin/fble-test --profile prgms/fble-bench.fble prgms > bench.prof

.PHONY: perf
perf:
	rm -f gmon.out out/fble/obj/*.gcda
	./out/fble/fble-test prgms/AllTests.fble prgms
	mkdir -p perf
	gprof out/fble/fble-test > perf/gprof.txt
	gcov out/fble/obj/eval.o out/fble/obj/compile.o out/fble/obj/ref.o out/fble/obj/value.o> perf/fble.gcov
	mv *.gcov perf/
	rm gmon.out

