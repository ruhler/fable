
.PHONY: all
all:
	ninja -v -f util/build.ninja

.PHONY: clean
clean: 
	rm -rf build

