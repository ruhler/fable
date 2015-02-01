
.PHONY: all
all:
	ninja -v -f build.ninja

.PHONY: clean
clean: 
	rm -rf build

