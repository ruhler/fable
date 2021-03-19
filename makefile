
.PHONY: all
all:
	tclsh build.tcl

.PHONY: clean
clean:
	rm -rf out/

.PHONY: checkerr
checkerr:
	cat `find out -name *.err`

.PHONY: foo
foo:
	./out/bin/fble-test --profile foo.fble prgms > bench.prof

.PHONY: test
test:
	./out/bin/fble-stdio prgms/Fble/Tests.fble prgms

.PHONY: bench
bench:
	./out/bin/fble-test prgms/Fble/Bench.fble prgms
	mkdir -p out/cov/bench
	gcov out/obj/*.o > fble.gcov
	mv *.gcov out/cov/bench/

.PHONY: benchprof
benchprof:
	#./out/bin/fble-test --profile prgms/Fble/Bench.fble prgms > bench.prof
	./out/bin/fble-stdio --profile bench.prof prgms/Fblf/Lib/Md5/Stdio.fble prgms > /dev/null

.PHONY: perf
perf:
	perf record -F 997 -d --call-graph dwarf ./out/bin/fble-test prgms/Fble/Bench.fble prgms/
	perf report -g folded,address,0.1,0 > perf.report.txt
	#perf report -g folded,address,callee,0.1,0 > perf.report.txt

.PHONY: prof
prof:
	./out/bin/fble-test prgms/Fble/Bench.fble prgms
	mkdir -p prof
	gprof out/bin/fble-test > prof/gprof.txt
	gcov out/obj/*.o > prof/fble.gcov
	mv *.gcov prof/
	rm gmon.out

ninja/build.ninja: build.ninja.tcl
	mkdir -p ninja
	tclsh $< > $@

.PHONY: ninja
ninja: ninja/build.ninja
	ninja -f $<
	tail -n 3 ninja/tests/summary.txt
