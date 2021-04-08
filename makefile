
default: all

out/build.ninja: build.ninja.tcl
	mkdir -p out
	tclsh $< > $@

.PHONY: foo
foo:
	ninja -f out/build.ninja -j 1 out/prgms/Sat/Aim.fble.o

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
	./out/bin/fble-stdio prgms/Fble/Bench.fble prgms

.PHONY: benchprof
benchprof:
	./out/bin/fble-test --profile prgms/Fble/Bench.fble prgms > bench.prof

