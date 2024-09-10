namespace eval "fbld" {
  # config.fbld
  build $::b/fbld/config.fbld \
    "$::s/fbld/config.fbld.tcl $::b/config.tcl" \
    "tclsh8.6 $::s/fbld/config.fbld.tcl > $::b/fbld/config.fbld"

  # version.fbld
  build $::b/fbld/version.fbld "$::s/fbld/version.fbld.sh" \
    "$::s/fbld/version.fbld.sh $::version > $::b/fbld/version.fbld"

  set ::default_fbld $::b/pkgs/fbld/bin/fbld

  # Builds a man page from an fbld @usage doc.
  proc ::fbld_man_usage { target source } {
    build $target \
      "$::default_fbld $::s/buildstamp $::b/fbld/version.fbld $::s/fbld/man.fbld $::s/fbld/usage.man.fbld $::s/fbld/usage.lib.fbld $::b/fbld/config.fbld $source" \
      "$::s/buildstamp --fbld BuildStamp | $::default_fbld - $::b/fbld/config.fbld $::b/fbld/version.fbld $::s/fbld/man.fbld $::s/fbld/usage.man.fbld $::s/fbld/usage.lib.fbld $source > $target"
  }

  # Builds a man page from an fbld doc comment.
  # @arg target - the target man file to produce.
  # @arg source - the C header file to extract the doc comment from.
  # @arg id - the name of the function to extract the docs for.
  proc ::fbld_man_dc { target source id } {
    build $target.fbld \
      "$source $::s/fbld/dcget.tcl" \
      "tclsh8.6 $::s/fbld/dcget.tcl $id < $source > $target.fbld"
    build $target \
      "$::default_fbld $::s/buildstamp $::b/fbld/version.fbld $::s/fbld/man.fbld $::s/fbld/dc.man.fbld $::b/fbld/config.fbld $target.fbld" \
      "$::s/buildstamp --fbld BuildStamp | $::default_fbld - $::b/fbld/config.fbld $::b/fbld/version.fbld $::s/fbld/man.fbld $::s/fbld/dc.man.fbld $target.fbld > $target"
  }

  # Check syntax of doc comments in the given file.
  # @arg target - the test to generate.
  # @arg source - the C header file to test doc comments in.
  proc ::fbld_check_dc { target source } {
    build $target.fbld \
      "$source $::s/fbld/dcget.tcl" \
      "tclsh8.6 $::s/fbld/dcget.tcl < $source > $target.fbld"
    test $target \
      "$::default_fbld $::s/fbld/dc.check.fbld $target.fbld" \
      "$::default_fbld $::s/fbld/dc.check.fbld $target.fbld"
  }

  # Builds usage help text.
  # @arg target - the name of the help text file to generate.
  # @arg source - the .fbld usage doc to generate the header from.
  proc ::fbld_help_usage { target source } {
    build $target.roff \
      "$::default_fbld $::s/buildstamp $::b/fbld/version.fbld $::s/fbld/roff.fbld $::s/fbld/usage.help.fbld $::s/fbld/usage.lib.fbld $::b/fbld/config.fbld $source" \
      "$::s/buildstamp --fbld BuildStamp | $::default_fbld - $::b/fbld/config.fbld $::b/fbld/version.fbld $::s/fbld/roff.fbld $::s/fbld/usage.help.fbld $::s/fbld/usage.lib.fbld $source > $target.roff"
    # Pass -c, -b, -u to grotty to escape sequences and backspaces in the output.
    build $target $target.roff \
      "groff -P -c -P -b -P -u -T utf8 < $target.roff > $target"
  }

  # Builds usage help text for use in an fble program.
  # @arg target - the .fble file to generate.
  # @arg source - the .fbld usage doc to generate the header from.
  proc ::fbld_help_fble_usage { target source } {
    build $target.roff \
      "$::default_fbld $::s/buildstamp $::b/fbld/version.fbld $::s/fbld/roff.fbld $::s/fbld/usage.help.fbld $::s/fbld/usage.lib.fbld $::b/fbld/config.fbld $source" \
      "$::s/buildstamp --fbld BuildStamp | $::default_fbld - $::b/fbld/config.fbld $::b/fbld/version.fbld $::s/fbld/roff.fbld $::s/fbld/usage.help.fbld $::s/fbld/usage.lib.fbld $source > $target.roff"
    # Pass -c, -b, -u to grotty to escape sequences and backspaces in the output.
    build $target.txt $target.roff \
      "groff -P -c -P -b -P -u -T ascii < $target.roff > $target.txt"
    build $target "$target.txt $::s/fbld/fblestr.sh" \
      "$::s/fbld/fblestr.sh $target.txt > $target"
  }

  proc ::fbld_html_doc { target sources } {
    build $target \
      "$::default_fbld $::s/buildstamp $::b/fbld/version.fbld $::s/fbld/html.fbld $sources" \
      "$::s/buildstamp --fbld BuildStamp | $::default_fbld - $::b/fbld/version.fbld $::s/fbld/html.fbld $sources > $target"
  }

  proc ::fbld_html_tutorial { target source } {
    fbld_html_doc $target [list $::s/tutorials/tutorial.lib.fbld $source]
  }

  # Fbld Spec www.
  ::fbld_html_doc $::b/www/fbld/fbld.html $::s/fbld/fbld.fbld
  www fbld/fbld.html

  # DocComments www.
  ::fbld_html_doc $::b/www/fbld/DocComments.html $::s/fbld/DocComments.fbld
  www fbld/DocComments.html

  # Test dcget.tcl
  build_tcl $::s/fbld/dcget_test/build.tcl

  # fbld binary.
  set objs [list]
  foreach {x} {alloc fbld eval markup parse vector} {
    obj $::b/fbld/$x.o $::s/fbld/$x.c "-I $::s/fbld"
    lappend objs $::b/fbld/$x.o
  }
  bin $::b/fbld/fbld $objs ""

  # Fbld spec tests.
  foreach {x} [build_glob $::s/fbld/SpecTests -tails -nocomplain -type f *.fbld] {
    # c-based fbld.
    test $::b/fbld/SpecTests/$x.c.tr \
      "$::b/fbld/fbld $::s/fbld/spec-test.run.tcl $::s/fbld/SpecTests.fbld $::s/fbld/SpecTests/$x" \
      "tclsh8.6 $::s/fbld/spec-test.run.tcl $::b/fbld/fbld $::s/fbld/SpecTests.fbld $::s/fbld/SpecTests/$x"

    # fble-based fbld.
    test $::b/fbld/SpecTests/$x.fble.tr \
      "$::b/pkgs/fbld/bin/fbld $::s/fbld/spec-test.run.tcl $::s/fbld/SpecTests.fbld $::s/fbld/SpecTests/$x" \
      "tclsh8.6 $::s/fbld/spec-test.run.tcl $::b/pkgs/fbld/bin/fbld $::s/fbld/SpecTests.fbld $::s/fbld/SpecTests/$x"
  }
}
