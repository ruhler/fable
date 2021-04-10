
default: all

out/build.ninja: build.ninja.tcl
	mkdir -p out
	tclsh $< > $@

.PHONY: foo
foo:
	ninja -f out/build.ninja -j 1 out/bin/fble-bench

.PHONY: all
all: out/build.ninja
	ninja -f out/build.ninja

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
	bash -c 'time ./out/bin/fble-stdio prgms/Fble/Bench.fble prgms'
	bash -c 'time ./out/bin/fble-bench'

.PHONY: benchprof
benchprof:
	./out/bin/fble-stdio --profile bench.prof prgms/Fble/Bench.fble prgms

