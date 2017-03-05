
.PHONY: all
all:
	tclsh build.tcl

.PHONY: clean
clean:
	rm -rf out/

.PHONY: checkerr
checkerr: all
	cat out/test/*.err

