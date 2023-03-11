namespace eval "include" {
  dist_s $::s/include/build.tcl
  foreach {x} [build_glob $::s/include/fble *.h] {
    dist_s $x
    install $x $::config::includedir/fble/[file tail $x]
  }

  build $::b/include/fble/FbleEval.fbld \
    "$::s/include/fble/fble-value.h $::s/fbld/dcget.tcl" \
    "tclsh8.6 $::s/fbld/dcget.tcl FbleEval < $::s/include/fble/fble-value.h > $::b/include/fble/FbleEval.fbld"
}
