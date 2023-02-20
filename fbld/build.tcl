namespace eval "fbld" {

  foreach x [build_glob $::s/fbld *] {
    dist_s $x
  }

  # Test suite for fbld implementation.
  test $::b/fbld/test.tr $::s/fbld/test.tcl \
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

  # Fbld Spec www.
  ::html_doc $::b/www/fbld/fbld.html $::s/fbld/fbld.fbld
  www $::b/www/fbld/fbld.html
}
