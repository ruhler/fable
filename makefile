
default: all

out/build.ninja: build.ninja.tcl
	mkdir -p out
	tclsh $< > $@

.PHONY: ninja
ninja:
	mkdir -p out
	tclsh build.ninja.tcl > out/build.ninja

.PHONY: foo
foo:
	ninja -f out/build.ninja -k 0
	#ninja -f out/build.ninja out/test/0-test/no-error/test-compiled.tr

.PHONY: all
all: out/build.ninja
	ninja -f out/build.ninja -j 2

.PHONY: clean
clean:
	rm -rf out/

.PHONY: full
full:
	rm -rf out/
	mkdir -p out
	tclsh build.ninja.tcl > out/build.ninja
	ninja -f out/build.ninja

.PHONY: bench
bench:
	bash -c 'time ./out/bin/fble-stdio prgms /Fble/Bench%'
	bash -c 'time ./out/bin/fble-bench'

.PHONY: benchprof
benchprof:
	./out/bin/fble-stdio --profile bench.prof prgms/Fble/Bench.fble prgms

.PHONY: asm
asm:
	./out/bin/fble-compile -s /Md5% prgms > foo.s
	as -o foo.o foo.s
