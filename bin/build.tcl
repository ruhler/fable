namespace eval "bin" {
  foreach {x} [glob $::s/bin/*.c] {
    set base [file rootname [file tail $x]]

    test $::b/bin/$base.fbld.tr \
      "$::s/bin/$base.fbld $::s/fbld/fbld.tcl $::s/fbld/core.tcl $::s/fbld/frontends/usage.tcl" \
      "tclsh8.6 $::s/fbld/frontends/usage.tcl $::s/bin/$base.fbld"

    # Generated header file for help usage text.
    build $::b/bin/$base.usage.h \
      "$::s/fbld/fbld.usage.help.tcl $::s/bin/$base.fbld" \
      "tclsh8.6 $::s/fbld/fbld.usage.help.tcl $::s/bin/$base.fbld > $::b/bin/$base.usage.h"

    # The binary.
    obj $::b/bin/$base.o $x "-I $::s/include -I $::b/bin" \
      $::b/bin/$base.usage.h
    bin $::b/bin/$base \
      "$::b/bin/$base.o $::b/lib/libfble.a" ""
    bin_cov $::b/bin/$base.cov \
      "$::b/bin/$base.o $::b/lib/libfble.cov.a" ""
    install_bin $::b/bin/$base

    # Man page.
    build $::b/bin/$base.1 \
      "$::s/fbld/fbld.usage.man.tcl $::s/bin/$base.fbld" \
      "tclsh8.6 $::s/fbld/fbld.usage.man.tcl $base $::s/bin/$base.fbld > $::b/bin/$base.1"
    install_man1 $::b/bin/$base.1
  }
}
