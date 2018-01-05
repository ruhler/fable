
.PHONY: all
all:
	tclsh build.tcl

.PHONY: clean
clean:
	rm -rf out/

.PHONY: checkerr
checkerr: all
	cat out/test/*.err

.PHONY: foo
foo:
	#./out/fbld/fbld-test prgms/AllTests.wnt prgms/ Test@AllTestsM
	./out/fbld/fbld-test prgms/Md5Debug.wnt prgms/ Debug@Md5M

