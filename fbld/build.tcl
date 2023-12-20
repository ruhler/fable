namespace eval "fbld" {
  # config.fbld
  build $::b/fbld/config.fbld \
    "$::s/fbld/config.fbld.tcl $::b/config.tcl" \
    "tclsh8.6 $::s/fbld/config.fbld.tcl > $::b/fbld/config.fbld"

  # version.fbld
  build $::b/fbld/version.fbld "" \
    "echo @define\[FbleVersion\]\[\]\[$::version\] @@ > $::b/fbld/version.fbld"

  # Builds a man page from an fbld @usage doc.
  proc ::fbld_man_usage { target source } {
    build $target \
      "$::b/pkgs/fbld/bin/fbld $::s/buildstamp $::b/fbld/version.fbld $::s/fbld/man.fbld $::s/fbld/usage.man.fbld $::s/fbld/usage.lib.fbld $::b/fbld/config.fbld $source" \
      "$::s/buildstamp --fbld BuildStamp | $::b/pkgs/fbld/bin/fbld - $::b/fbld/config.fbld $::b/fbld/version.fbld $::s/fbld/man.fbld $::s/fbld/usage.man.fbld $::s/fbld/usage.lib.fbld $source > $target"
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
      "$::b/pkgs/fbld/bin/fbld $::s/buildstamp $::b/fbld/version.fbld $::s/fbld/man.fbld $::s/fbld/dc.man.fbld $::b/fbld/config.fbld $target.fbld" \
      "$::s/buildstamp --fbld BuildStamp | $::b/pkgs/fbld/bin/fbld - $::b/fbld/config.fbld $::b/fbld/version.fbld $::s/fbld/man.fbld $::s/fbld/dc.man.fbld $target.fbld > $target"
  }

  # Check syntax of doc comments in the given file.
  # @arg target - the test to generate.
  # @arg source - the C header file to test doc comments in.
  proc ::fbld_check_dc { target source } {
    build $target.fbld \
      "$source $::s/fbld/dcget.tcl" \
      "tclsh8.6 $::s/fbld/dcget.tcl < $source > $target.fbld"
    test $target \
      "$::b/pkgs/fbld/bin/fbld $::s/fbld/dc.check.fbld $target.fbld" \
      "$::b/pkgs/fbld/bin/fbld $::s/fbld/dc.check.fbld $target.fbld"
  }

  # Builds usage help text.
  # @arg target - the name of the help text file to generate.
  # @arg source - the .fbld usage doc to generate the header from.
  proc ::fbld_help_usage { target source } {
    build $target.roff \
      "$::b/pkgs/fbld/bin/fbld $::s/buildstamp $::b/fbld/version.fbld $::s/fbld/roff.fbld $::s/fbld/usage.help.fbld $::s/fbld/usage.lib.fbld $::b/fbld/config.fbld $source" \
      "$::s/buildstamp --fbld BuildStamp | $::b/pkgs/fbld/bin/fbld - $::b/fbld/config.fbld $::b/fbld/version.fbld $::s/fbld/roff.fbld $::s/fbld/usage.help.fbld $::s/fbld/usage.lib.fbld $source > $target.roff"
    # Pass -c, -b, -u to grotty to escape sequences and backspaces in the output.
    build $target $target.roff \
      "groff -P -c -P -b -P -u -T utf8 < $target.roff > $target"
  }

  proc ::fbld_html_doc { target sources } {
    build $target \
      "$::b/pkgs/fbld/bin/fbld $::s/buildstamp $::b/fbld/version.fbld $::s/fbld/html.fbld $sources" \
      "$::s/buildstamp --fbld BuildStamp | $::b/pkgs/fbld/bin/fbld - $::b/fbld/version.fbld $::s/fbld/html.fbld $sources > $target"
  }

  proc ::fbld_html_tutorial { target source } {
    fbld_html_doc $target [list $::s/tutorials/tutorial.lib.fbld $source]
  }

  # Fbld Spec www.
  ::fbld_html_doc $::b/www/fbld/fbld.html $::s/fbld/fbld.fbld
  www $::b/www/fbld/fbld.html

  # DocComments www.
  ::fbld_html_doc $::b/www/fbld/DocComments.html $::s/fbld/DocComments.fbld
  www $::b/www/fbld/DocComments.html

  # Test dcget.tcl
  build_tcl $::s/fbld/dcget_test/build.tcl
}
