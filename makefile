
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
	./out/bin/fble-test --profile prgms/stack-smasher.fble > smash.prof
	#./out/bin/fble-stdio prgms/fble-sudoku.fble prgms/
	#./out/bin/fble-stdio prgms/fble-tictactoe.fble prgms/
	#./out/bin/fble-stdio prgms/fble-tests.fble prgms/
	#./out/bin/fble-test --profile prgms/fble-bench.fble prgms > bench.prof

.PHONY: perf
perf:
	rm -f gmon.out out/fble/obj/*.gcda
	./out/bin/fble-test --profile prgms/fble-bench.fble prgms
	mkdir -p perf
	gprof out/bin/fble-test > perf/gprof.txt
	gcov out/obj/eval.o out/obj/compile.o out/obj/ref.o out/obj/value.o> perf/fble.gcov
	mv *.gcov perf/
	rm gmon.out

