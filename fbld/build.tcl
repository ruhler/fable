namespace eval "fbld" {

  dist_s $::s/fbld/build.tcl
  dist_s $::s/fbld/cdata.tcl
  dist_s $::s/fbld/check.tcl
  dist_s $::s/fbld/config.fbld.tcl
  dist_s $::s/fbld/dcget.tcl
  dist_s $::s/fbld/dc.man.tcl
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
      [list $source $::s/fbld/fbld.tcl $::s/fbld/runfbld.tcl \
        $::s/fbld/config.fbld.tcl $::b/config.tcl {*}$args {*}$deps] \
      "tclsh8.6 $::s/fbld/runfbld.tcl $::s/fbld/config.fbld.tcl [join $args] < $source > $target"
  }

  # Builds an html file from an fbld @doc.
  #
  # args is additional optional fbld processing scripts.
  proc ::html_doc { target source args } {
    fbld $target $source \
      "$::s/fbld/fbld.lang $::s/spec/fble.lang" \
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
    fbld $target $source "" \
      [list $::s/fbld/usage.man.tcl $::s/fbld/man.tcl $::s/fbld/usage.lib.tcl]
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
  proc ::header_usage { target source id } {
    fbld $target.roff $source "" \
      [list $::s/fbld/usage.help.tcl $::s/fbld/roff.tcl $::s/fbld/usage.lib.tcl]
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
