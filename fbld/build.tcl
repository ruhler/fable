namespace eval "fbld" {
  # config.fbld
  build $::b/fbld/config.fbld \
    "$::s/fbld/config.fbld.tcl $::b/config.tcl" \
    "tclsh8.6 $::s/fbld/config.fbld.tcl > $::b/fbld/config.fbld"

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
  proc ::fbld_man_dc { target source id } {
    build $target.fbld \
      "$source $::s/fbld/dcget.tcl" \
      "tclsh8.6 $::s/fbld/dcget.tcl $id < $source > $target.fbld"
    build $target \
      "$::b/pkgs/fbld/fbld $::s/buildstamp $::b/fbld/version.fbld $::s/fbld/man.fbld $::s/fbld/dc.man.fbld $::b/fbld/config.fbld $target.fbld" \
      "$::s/buildstamp --fbld BuildStamp | $::b/pkgs/fbld/fbld - $::b/fbld/config.fbld $::b/fbld/version.fbld $::s/fbld/man.fbld $::s/fbld/dc.man.fbld $target.fbld > $target"
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
