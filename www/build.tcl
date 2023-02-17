namespace eval "www" {
  set wwws [list]

  # README
  ::html_doc $::b/www/index.html $::s/README.fbld
  lappend wwws $::b/www/index.html

  # Fbld spec
  ::html_doc $::b/www/fbld/fbld.html $::s/fbld/fbld.fbld
  lappend wwws $::b/www/fbld/fbld.html

  # Fble spec
  ::html_doc $::b/www/spec/fble.html $::s/spec/fble.fbld $::s/spec/fble.fbld.tcl
  lappend wwws $::b/www/spec/fble.html

  # Tutorial table of contents
  ::html_doc $::b/www/tutorials/Tutorials.html $::s/tutorials/Tutorials.fbld
  lappend wwws $::b/www/tutorials/Tutorials.html

  # Tutorials
  lappend ::build_ninja_deps $::s/tutorials
  foreach {x} [glob $::s/tutorials/*.fbld] {
    if [string equal $x $::s/tutorials/Tutorials.fbld] {
      continue
    }

    set base [file rootname [file tail $x]]
    ::html_tutorial $::b/www/tutorials/$base.html $::s/tutorials/$base.fbld
    lappend wwws $::b/www/tutorials/$base.html
  }

  phony "www" $wwws
}
