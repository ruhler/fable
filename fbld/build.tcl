namespace eval "fbld" {
  # Test suite for fbld implementation.
  test $::b/fbld/test.tr $::s/fbld/test.tcl \
    "tclsh8.6 $::s/fbld/test.tcl"
}
