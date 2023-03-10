namespace eval "fbld" {

  dist_s $::s/fbld/build.tcl
  dist_s $::s/fbld/cdata.tcl
  dist_s $::s/fbld/check.tcl
  dist_s $::s/fbld/fbld.fbld
  dist_s $::s/fbld/fbld.lang
  dist_s $::s/fbld/fbld.tcl
  dist_s $::s/fbld/html.tcl
  dist_s $::s/fbld/man.tcl
  dist_s $::s/fbld/roff.tcl
  dist_s $::s/fbld/runfbld.tcl
  dist_s $::s/fbld/test.tcl
  dist_s $::s/fbld/tutorial.tcl
  dist_s $::s/fbld/usage.help.tcl
  dist_s $::s/fbld/usage.lib.tcl
  dist_s $::s/fbld/usage.man.tcl

  # Test suite for fbld implementation.
  test $::b/fbld/test.tr "$::s/fbld/fbld.tcl $::s/fbld/test.tcl" \
    "tclsh8.6 $::s/fbld/test.tcl"

  # Processes an fbld file.
  #
  #   target - the file to generate
  #   source - the .fbld file
  #   deps - additional dependencies
  #   args - list of fbld processing scripts to use.
  proc ::fbld { target source deps args } {
    build $target \
      "$source $::s/fbld/fbld.tcl $::s/fbld/runfbld.tcl $args" \
      "tclsh8.6 $::s/fbld/fbld.tcl $::s/fbld/runfbld.tcl $args < $source > $target"
  }

  # Builds an html file from an fbld @doc.
  #
  # args is additional optional fbld processing scripts.
  proc ::html_doc { target source args } {
    fbld $target $source \
      "$::s/fbld/fbld-lang $::s/spec/fble.lang" \
      [list $::s/fbld/html.tcl {*}$args]
  }

  # Builds an html file from an fbld @tutorial.
  #
  # args is additional optional fbld processing scripts.
  proc ::html_tutorial { target source args } {
    html_doc $target $source [list $::s/fbld/tutorial.tcl {*}$args]
  }

  # Builds a man page from an fbld @usage doc.
  proc ::man_usage { target source } {
    build $target \
      "$source $::s/fbld/usage.man.tcl $::s/fbld/fbld.tcl $::s/fbld/runfbld.tcl $::s/fbld/man.tcl $::s/fbld/usage.lib.tcl" \
      "tclsh8.6 $::s/fbld/runfbld.tcl $::s/fbld/usage.man.tcl $::s/fbld/man.tcl $::s/fbld/usage.lib.tcl < $source > $target"
  }

  # Builds C header file defining help usage text.
  # @arg target - the name of the .h file to generate.
  # @arg source - the .fbld usage doc to generate the header from.
  # @arg id - the C identifier to declare in the generated header.
  proc ::header_usage { target source id } {
    build $target.roff \
      "$source $::s/fbld/usage.help.tcl $::s/fbld/fbld.tcl $::s/fbld/runfbld.tcl $::s/fbld/roff.tcl $::s/fbld/usage.lib.tcl" \
      "tclsh8.6 $::s/fbld/runfbld.tcl $::s/fbld/usage.help.tcl $::s/fbld/roff.tcl $::s/fbld/usage.lib.tcl < $source > $target.roff"
    build $target.txt $target.roff \
      "GROFF_NO_SGR=1 groff -T ascii < $target.roff > $target.txt"
    build $target \
      "$::s/fbld/cdata.tcl $target.txt" \
      "tclsh8.6 $::s/fbld/cdata.tcl $id < $target.txt > $target"
  }

  # Fbld Spec www.
  ::html_doc $::b/www/fbld/fbld.html $::s/fbld/fbld.fbld
  www $::b/www/fbld/fbld.html
}
