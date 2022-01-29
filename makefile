
default: all

out/build.ninja: build.ninja.tcl
	tclsh $<

.PHONY: ninja
ninja:
	tclsh build.ninja.tcl

.PHONY: foo
foo:
	ninja -f out/build.ninja out/test/8.3-abstract-cast/regression/renamed-token-type/test.tr

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
	tclsh build.ninja.tcl
	ninja -f out/build.ninja

.PHONY: test
test:
	./out/bin/fble-stdio prgms /Fble/Tests%

.PHONY: bench
bench:
	ninja -f out/build.ninja out/bin/fble-stdio out/bin/fble-bench
	bash -c 'time ./out/bin/fble-stdio prgms /Fble/Bench%'
	bash -c 'time ./out/bin/fble-bench'

.PHONY: benchprof
benchprof:
	ninja -f out/build.ninja
	./out/bin/fble-bench --profile bench.prof

.PHONY: perf
perf:
	ninja -f out/build.ninja out/bin/fble-bench out/bin/fble-perf-profile
	perf record -F 997 -d -g ./out/bin/fble-bench
	perf report -q -g folded,count,0 | ./out/bin/fble-perf-profile > bench.perf.txt

.PHONY: asm
asm:
	./out/bin/fble-compile -s /Md5% prgms > foo.s
	as -o foo.o foo.s
