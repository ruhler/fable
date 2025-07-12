namespace eval "fbld" {
  # config.fbld
  build $::b/fbld/config.fbld \
    "$::s/fbld/config.fbld.tcl $::br/config.tcl" \
    "tclsh8.6 $::s/fbld/config.fbld.tcl > $::b/fbld/config.fbld"

  # version.fbld
  build $::b/fbld/version.fbld "$::s/fbld/version.fbld.sh" \
    "$::s/fbld/version.fbld.sh $::version > $::b/fbld/version.fbld"

  set ::fbld $::b/fbld/fbld

  # Builds a man page from an fbld @usage doc.
  proc ::fbld_man_usage { target source } {
    build $target \
      "$::fbld $::s/buildstamp $::b/fbld/version.fbld $::s/fbld/man.fbld $::s/fbld/usage.man.fbld $::s/fbld/usage.lib.fbld $::b/fbld/config.fbld $source" \
      "$::s/buildstamp --fbld BuildStamp | $::fbld - $::b/fbld/config.fbld $::b/fbld/version.fbld $::s/fbld/man.fbld $::s/fbld/usage.man.fbld $::s/fbld/usage.lib.fbld $source > $target"
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
      "$::fbld $::s/buildstamp $::b/fbld/version.fbld $::s/fbld/man.fbld $::s/fbld/dc.man.fbld $::b/fbld/config.fbld $target.fbld" \
      "$::s/buildstamp --fbld BuildStamp | $::fbld - $::b/fbld/config.fbld $::b/fbld/version.fbld $::s/fbld/man.fbld $::s/fbld/dc.man.fbld $target.fbld > $target"
  }

  # Check syntax of doc comments in the given file.
  # @arg target - the test to generate.
  # @arg source - the C header file to test doc comments in.
  proc ::fbld_check_dc { target source } {
    build $target.fbld \
      "$source $::s/fbld/dcget.tcl" \
      "tclsh8.6 $::s/fbld/dcget.tcl < $source > $target.fbld"
    test $target \
      "$::fbld $::s/fbld/dc.check.fbld $target.fbld" \
      "$::fbld $::s/fbld/dc.check.fbld $target.fbld"
  }

  # Builds C header file defining help usage text.
  # @arg target - the name of the .h file to generate.
  # @arg source - the .fbld usage doc to generate the header from.
  # @arg id - the C identifier to declare in the generated header.
  # @arg golden - the name of a golden reference .txt file with the usage
  #               text to generate.
  #
  # To avoid cyclic build dependencies, a golden file should be specified with
  # usage header text. The golden file will be used to generated the header,
  # and a test will be added that it matches what would have been generated
  # from source.
  proc ::fbld_header_usage { target source id } {
    build $target.roff \
      "$::fbld $::s/buildstamp $::b/fbld/version.fbld $::s/fbld/roff.fbld $::s/fbld/usage.help.fbld $::s/fbld/usage.lib.fbld $::b/fbld/config.fbld $source" \
      "$::s/buildstamp --fbld BuildStamp | $::fbld - $::b/fbld/config.fbld $::b/fbld/version.fbld $::s/fbld/roff.fbld $::s/fbld/usage.help.fbld $::s/fbld/usage.lib.fbld $source > $target.roff"
    # Pass -c, -b, -u to grotty to escape sequences and backspaces in the output.
    build $target.txt $target.roff \
      "groff -P -c -P -b -P -u -T ascii < $target.roff > $target.txt"
    build $target \
      "$::s/fbld/cdata.tcl $target.txt" \
      "tclsh8.6 $::s/fbld/cdata.tcl $id < $target.txt > $target"
  }

  # Builds usage help text for use in an fble program.
  # @arg target - the .fble file to generate.
  # @arg source - the .fbld usage doc to generate the header from.
  proc ::fbld_help_fble_usage { target source } {
    build $target.roff \
      "$::fbld $::s/buildstamp $::b/fbld/version.fbld $::s/fbld/roff.fbld $::s/fbld/usage.help.fbld $::s/fbld/usage.lib.fbld $::b/fbld/config.fbld $source" \
      "$::s/buildstamp --fbld BuildStamp | $::fbld - $::b/fbld/config.fbld $::b/fbld/version.fbld $::s/fbld/roff.fbld $::s/fbld/usage.help.fbld $::s/fbld/usage.lib.fbld $source > $target.roff"
    # Pass -c, -b, -u to grotty to escape sequences and backspaces in the output.
    build $target.txt $target.roff \
      "groff -P -c -P -b -P -u -T ascii < $target.roff > $target.txt"
    build $target "$target.txt $::s/fbld/fblestr.sh" \
      "$::s/fbld/fblestr.sh $target.txt > $target"
  }

  proc ::fbld_html_doc { target sources } {
    build $target \
      "$::fbld $::s/buildstamp $::b/fbld/version.fbld $::s/fbld/html.fbld $sources" \
      "$::s/buildstamp --fbld BuildStamp | $::fbld - $::b/fbld/version.fbld $::s/fbld/html.fbld $sources > $target"
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
  set objs_cov [list]
  foreach {x} {alloc fbld eval markup parse vector} {
    obj $::b/fbld/$x.o $::s/fbld/$x.c "-I $::s/fbld"
    obj_cov $::b/fbld/$x.cov.o $::s/fbld/$x.c "-I $::s/fbld"
    lappend objs $::b/fbld/$x.o
    lappend objs_cov $::b/fbld/$x.cov.o
  }

  # buildstamp
  build "$::b/fbld/buildstamp.c" "$::s/buildstamp $objs" \
    "$::s/buildstamp -c FbldBuildStamp > $::b/fbld/buildstamp.c"
  obj $::b/fbld/buildstamp.o $::b/fbld/buildstamp.c ""
  obj_cov $::b/fbld/buildstamp.cov.o $::b/fbld/buildstamp.c ""
  lappend objs $::b/fbld/buildstamp.o
  lappend objs_cov $::b/fbld/buildstamp.cov.o

  bin $::b/fbld/fbld $objs "" ""
  bin_cov $::b/fbld/fbld.cov $objs_cov "" ""
  set ::fbld_objs_cov $objs_cov

  install $::b/fbld/fbld $::config::bindir/fbld
  fbld_man_usage $::b/fbld/fbld.1 $::s/fbld/fbld.1.fbld
  install $::b/fbld/fbld.1 $::config::mandir/man1/fbld.1

  # Fbld spec tests.
  set fbld_spec_tests [list]
  foreach {x} [build_glob $::s/fbld/SpecTests -tails -nocomplain -type f *.fbld] {
    # c-based fbld.
    lappend fbld_spec_tests $::b/fbld/SpecTests/$x.c.tr
    test $::b/fbld/SpecTests/$x.c.tr \
      "$::b/fbld/fbld.cov $::s/fbld/spec-test.run.tcl $::s/fbld/SpecTests.fbld $::s/fbld/SpecTests/$x" \
      "tclsh8.6 $::s/fbld/spec-test.run.tcl $::b/fbld/fbld.cov $::s/fbld/SpecTests.fbld $::s/fbld/SpecTests/$x"
  }

  # Code coverage. Mark it as 'testsuite' so it gets built along with all the
  # other test cases.
  # TODO: There should be a better way to say when it gets built.
  testsuite $::b/fbld/cov/gcov.txt "$::fbld_objs_cov $fbld_spec_tests" \
    "gcov $::fbld_objs_cov > $::b/fbld/cov/gcov.txt && mv *.gcov $::b/fbld/cov"
}
