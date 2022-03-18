
default: all

out/build.ninja: build.tcl
	tclsh $<

.PHONY: ninja
ninja:
	tclsh build.tcl

.PHONY: all
all: out/build.ninja
	ninja -f out/build.ninja -k 0

.PHONY: clean
clean:
	rm -rf out/
