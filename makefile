
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
	./out/fble/fble-test prgms/AllTests.fble prgms

.PHONY: perf
perf:
	rm -f gmon.out out/fble/obj/*.gcda
	./out/fble/fble-test prgms/AllTests.fble prgms
	mkdir -p perf
	gprof out/fble/fble-test > perf/gprof.txt
	gcov out/fble/obj/{eval,compile,ref,value}.o > perf/fble.gcov
	mv *.gcov perf/
	rm gmon.out

