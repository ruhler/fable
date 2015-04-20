
.PHONY: all
all: calvisus
	tclsh bathylus/bathylus.tcl && echo PASSED

.PHONY: calvisus
calvisus:
	gcc calvisus/*.h


