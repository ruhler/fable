namespace eval "include" {
  lappend ::build_ninja_deps "$::s/include/fble"
  foreach {x} [glob $::s/include/fble/*.h] {
    install $x $::config::includedir/fble/[file tail $x]
  }
}
