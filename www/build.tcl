namespace eval "www" {
  set wwws [list]

  build "$::b/www/index.html" "$::s/README.adoc" \
    "asciidoc -o $::b/www/index.html $::s/README.adoc"
  lappend wwws $::b/www/index.html

  build "$::b/www/spec/fble.html" "$::s/spec/fble.adoc" \
    "asciidoc -o $::b/www/spec/fble.html $::s/spec/fble.adoc"
  lappend wwws $::b/www/spec/fble.html

  # Tutorials
  lappend ::build_ninja_deps $::s/tutorials
  foreach {x} [glob $::s/tutorials/*.fbld] {
    set base [file rootname [file tail $x]]
    build $::b/www/tutorials/$base.html \
      "$::s/tutorials/$base.fbld $::s/fbld/fbld.tcl $::s/fbld/core.tcl $::s/fbld/frontends/tutorial.tcl $::s/fbld/backends/html.tcl" \
      "tclsh8.6 $::s/fbld/frontends/tutorial.tcl --html < $::s/tutorials/$base.fbld > $::b/www/tutorials/$base.html"
    lappend wwws $::b/www/tutorials/$base.html
  }

  phony "www" $wwws
}
