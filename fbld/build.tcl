namespace eval "fbld" {
  # Test suite for fbld implementation.
  test $::b/fbld/test.tr "$::s/fbld/fbld.tcl $::s/fbld/test.tcl" \
    "tclsh8.6 $::s/fbld/test.tcl"

  # version.fbld.tcl
  build $::b/fbld/version.fbld.tcl "" \
    "echo proc inline_FbleVersion {} { inline_ $::version } > $::b/fbld/version.fbld.tcl"

  # config.fbld
  build $::b/fbld/config.fbld \
    "$::s/fbld/config.fbld.tcl $::b/config.tcl" \
    "tclsh8.6 $::s/fbld/config.fbld.tcl > $::b/fbld/config.fbld"

  # Processes an fbld file.
  #
  #   target - the file to generate
  #   source - the .fbld file
  #   deps - additional dependencies
  #   args - list of fbld processing scripts to use.
  proc ::fbld { target source deps args } {
    build $target \
      [list $source $::s/fbld/fbld.tcl $::s/fbld/runfbld.tcl \
        $::s/fbld/build.lib.tcl $::b/fbld/version.fbld.tcl \
        $::b/config.tcl {*}$args {*}$deps] \
      "tclsh8.6 $::s/fbld/runfbld.tcl $::s/fbld/build.lib.tcl $::b/fbld/version.fbld.tcl [join $args] < $source > $target"
  }

  # Builds a man page from an fbld @usage doc.
  proc ::fbld_man_usage { target source } {
    build $target \
      "$::b/pkgs/fbld/fbld $::s/buildstamp $::b/fbld/version.fbld $::s/fbld/man.fbld $::s/fbld/usage.man.fbld $::s/fbld/usage.lib.fbld $::b/fbld/config.fbld $source" \
      "$::s/buildstamp --fbld BuildStamp | $::b/pkgs/fbld/fbld - $::b/fbld/config.fbld $::b/fbld/version.fbld $::s/fbld/man.fbld $::s/fbld/usage.man.fbld $::s/fbld/usage.lib.fbld $source > $target"
  }

  # Builds a man page from an fbld doc comment.
  # @arg target - the target man file to produce.
  # @arg source - the C header file to extract the doc comment from.
  # @arg id - the name of the function to extract the docs for.
  proc ::man_dc { target source id } {
    build $target.fbld \
      "$source $::s/fbld/dcget.tcl" \
      "tclsh8.6 $::s/fbld/dcget.tcl $id < $source > $target.fbld"
    fbld $target $target.fbld "" \
      [list $::s/fbld/dc.man.tcl $::s/fbld/man.tcl]
  }

  # Builds C header file defining help usage text.
  # @arg target - the name of the .h file to generate.
  # @arg source - the .fbld usage doc to generate the header from.
  # @arg id - the C identifier to declare in the generated header.
  proc ::fbld_header_usage { target source id } {
    build $target.roff \
      "$::b/pkgs/fbld/fbld $::s/buildstamp $::b/fbld/version.fbld $::s/fbld/roff.fbld $::s/fbld/usage.help.fbld $::s/fbld/usage.lib.fbld $::b/fbld/config.fbld $source" \
      "$::s/buildstamp --fbld BuildStamp | $::b/pkgs/fbld/fbld - $::b/fbld/config.fbld $::b/fbld/version.fbld $::s/fbld/roff.fbld $::s/fbld/usage.help.fbld $::s/fbld/usage.lib.fbld $source > $target.roff"
    build $target.txt $target.roff \
      "GROFF_NO_SGR=1 groff -T ascii < $target.roff > $target.txt"
    build $target \
      "$::s/fbld/cdata.tcl $target.txt" \
      "tclsh8.6 $::s/fbld/cdata.tcl $id < $target.txt > $target"
  }
}
