namespace eval "bin" {
  foreach {x} [glob $::s/bin/*.c] {
    set base [file rootname [file tail $x]]

    test $::b/bin/$base.fbld.tr \
      "$::s/bin/$base.fbld $::s/fbld/fbld.tcl $::s/fbld/runfbld.tcl $::s/fbld/check.tcl $::s/fbld/usage.man.tcl" \
      "tclsh8.6 $::s/fbld/runfbld.tcl $::s/fbld/usage.man.tcl $::s/fbld/check.tcl < $::s/bin/$base.fbld"

    # Generated header file for help usage text.
    build $::b/bin/$base.roff \
      "$::s/bin/$base.fbld $::s/fbld/usage.help.tcl $::s/fbld/fbld.tcl $::s/fbld/runfbld.tcl $::s/fbld/roff.tcl" \
      "tclsh8.6 $::s/fbld/runfbld.tcl $::s/fbld/usage.help.tcl $::s/fbld/roff.tcl < $::s/bin/$base.fbld > $::b/bin/$base.roff"
    build $::b/bin/$base.txt $::b/bin/$base.roff \
      "groff -T ascii < $::b/bin/$base.roff > $::b/bin/$base.txt"
    build $::b/bin/$base.usage.h \
      "$::s/fbld/cdata.tcl $::b/bin/$base.txt" \
      "tclsh8.6 $::s/fbld/cdata.tcl fbldUsageHelpText < $::b/bin/$base.txt > $::b/bin/$base.usage.h"

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
      "$::s/bin/$base.fbld $::s/fbld/usage.man.tcl $::s/fbld/fbld.tcl $::s/fbld/runfbld.tcl $::s/fbld/man.tcl" \
      "tclsh8.6 $::s/fbld/runfbld.tcl $::s/fbld/usage.man.tcl $::s/fbld/man.tcl < $::s/bin/$base.fbld > $::b/bin/$base.1"
    install_man1 $::b/bin/$base.1
  }
}
