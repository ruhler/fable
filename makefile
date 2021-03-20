
default: all

ninja/build.ninja: build.ninja.tcl
	mkdir -p ninja
	tclsh $< > $@

.PHONY: all
all: ninja/build.ninja
	ninja -f ninja/build.ninja
	tail -n 3 ninja/test/summary.txt

.PHONY: clean
clean:
	rm -rf ninja/

.PHONY: full
full:
	rm -rf ninja/
	mkdir -p ninja
	tclsh $< > $@
	ninja -f ninja/build.ninja
	tail -n 3 ninja/test/summary.txt

.PHONY: bench
bench:
	./ninja/bin/fble-test prgms/Fble/Bench.fble prgms

.PHONY: benchprof
benchprof:
	./ninja/bin/fble-test --profile prgms/Fble/Bench.fble prgms > bench.prof

