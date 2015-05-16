
.PHONY: all
all:
	tclsh build.tcl

.PHONY: clean
clean:
	rm -rf out/

.PHONY: checkerr
checkerr: all
	cat out/????e-*.got

.PHONY: doc
doc: build/calvisus.html
	
build/calvisus.html: spec/calvisus.txt
	mkdir -p out
	XML_CATALOG_FILES="/pkg/docbook/docbook-xsl-1.76.1/catalog.xml /pkg/docbook/4.5/catalog.xml" a2x -v -f xhtml -D out spec/calvisus.txt

