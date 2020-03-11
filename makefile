
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
	#./out/bin/fble-test --profile prgms/stack-smasher.fble > smash.prof
	#./out/bin/fble-stdio prgms/fble-tictactoe.fble prgms
	#./out/bin/fble-stdio prgms/fble-tests.fble prgms
	#./out/bin/fble-test --profile prgms/fble-bench.fble prgms > bench.prof
	#./out/bin/fble-test foo.fble prgms
	#./out/bin/fble-test prgms/fble-Snake.fble prgms
	./out/bin/fble-Snake prgms/GameOfLife/UI.fble prgms

.PHONY: perf
perf:
	perf record -F 997 -d --call-graph dwarf ./out/bin/fble-test prgms/fble-bench.fble prgms/
	perf report -g folded,address,0.1,0 > perf.report.txt
	#perf report -g folded,address,callee,0.1,0 > perf.report.txt

.PHONY: prof
prof:
	rm -f gmon.out out/fble/obj/*.gcda
	./out/bin/fble-test --profile prgms/fble-bench.fble prgms
	mkdir -p prof
	gprof out/bin/fble-test > prof/gprof.txt
	gcov out/obj/eval.o out/obj/compile.o out/obj/ref.o out/obj/value.o> prof/fble.gcov
	mv *.gcov prof/
	rm gmon.out

