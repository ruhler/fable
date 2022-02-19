
default: all

out/build.ninja: build.ninja.tcl
	tclsh $<

.PHONY: ninja
ninja:
	tclsh build.ninja.tcl

.PHONY: all
all: out/build.ninja
	ninja -f out/build.ninja

.PHONY: clean
clean:
	rm -rf out/
