namespace eval "include" {
  lappend ::build_ninja_deps "$::s/include/fble"
  foreach {x} [glob $::s/include/fble/*.h] {
    install_header $x
  }

  # Doxygen
  lappend ::build_ninja_deps "$::s/include/fble"
  build $::b/include/doxygen.log \
    "$::s/test/log $::s/include/DoxygenLayout.xml $::b/include/Doxyfile [glob $::s/include/fble/*.h]" \
    "$::s/test/log $::b/include/doxygen.log doxygen $::b/include/Doxyfile"

}
