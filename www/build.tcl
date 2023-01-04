namespace eval "www" {
  set wwws [list]

  # README
  build $::b/www/index.html \
    "$::s/README.fbld $::s/fbld/fbld.tcl $::s/fbld/runfbld.tcl $::s/fbld/html.tcl" \
    "tclsh8.6 $::s/fbld/runfbld.tcl $::s/fbld/html.tcl < $::s/README.fbld > $::b/www/index.html"
  lappend wwws $::b/www/index.html

  # Fbld spec
  build $::b/www/fbld/fbld.html \
    "$::s/fbld/fbld.fbld $::s/fbld/fbld.tcl $::s/fbld/runfbld.tcl $::s/fbld/html.tcl" \
    "tclsh8.6 $::s/fbld/runfbld.tcl $::s/fbld/html.tcl < $::s/fbld/fbld.fbld > $::b/www/fbld/fbld.html"
  lappend wwws $::b/www/fbld/fbld.html

  # Fble spec
  build $::b/www/spec/fble.html \
    "$::s/spec/fble.fbld $::s/spec/fble.fbld.tcl $::s/fbld/fbld.tcl $::s/fbld/runfbld.tcl $::s/fbld/html.tcl" \
    "tclsh8.6 $::s/fbld/runfbld.tcl $::s/spec/fble.fbld.tcl $::s/fbld/html.tcl < $::s/spec/fble.fbld > $::b/www/spec/fble.html"
  lappend wwws $::b/www/spec/fble.html

  # Tutorial table of contents
  build "$::b/www/tutorials/index.html" "$::s/tutorials/Tutorials.adoc" \
    "asciidoc -o $::b/www/tutorials/index.html $::s/tutorials/Tutorials.adoc"
  build $::b/www/tutorials/Tutorials.html \
    "$::s/tutorials/Tutorials.fbld $::s/fbld/fbld.tcl $::s/fbld/runfbld.tcl $::s/fbld/html.tcl" \
    "tclsh8.6 $::s/fbld/runfbld.tcl $::s/fbld/html.tcl < $::s/tutorials/Tutorials.fbld > $::b/www/tutorials/Tutorials.html"
  lappend wwws $::b/www/tutorials/Tutorials.html

  # Tutorials
  lappend ::build_ninja_deps $::s/tutorials
  foreach {x} [glob $::s/tutorials/*.fbld] {
    if [string equal $x $::s/tutorials/Tutorials.fbld] {
      continue
    }

    set base [file rootname [file tail $x]]
    build $::b/www/tutorials/$base.html \
      "$::s/tutorials/$base.fbld $::s/fbld/fbld.tcl $::s/fbld/runfbld.tcl $::s/fbld/tutorial.tcl $::s/fbld/html.tcl" \
      "tclsh8.6 $::s/fbld/runfbld.tcl $::s/fbld/tutorial.tcl $::s/fbld/html.tcl < $::s/tutorials/$base.fbld > $::b/www/tutorials/$base.html"
    lappend wwws $::b/www/tutorials/$base.html
  }

  phony "www" $wwws
}
