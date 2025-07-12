# Exports:
#   ::fble_objs_cov - list of .o files used in libfble.cov.
namespace eval "lib" {
  set objs [list]
  set objs_cov [list]

  # config.h
  build $::b/lib/config.h \
    "$::s/lib/config.h.tcl $::br/config.tcl" \
    "tclsh8.6 $::s/lib/config.h.tcl > $::b/lib/config.h"

  # parse.tab.c
  set report $::b/lib/parse.tab.report.txt
  set cmd "bison --report=all --report-file=$report -o $::b/lib/parse.tab.c $::s/lib/parse.y"
  build "$::b/lib/parse.tab.c $report" "$::s/lib/parse.y" $cmd

  # parse.tab.o
  obj $::b/lib/parse.tab.o $::b/lib/parse.tab.c "-I $::s/include -I $::s/lib"
  obj_cov $::b/lib/parse.tab.cov.o $::b/lib/parse.tab.c "-I $::s/include -I $::s/lib"
  lappend objs $::b/lib/parse.tab.o
  lappend objs_cov $::b/lib/parse.tab.cov.o

  # general .o files
  foreach {x} [build_glob $::s/lib -tails "*.c"] {
    set object $::b/lib/[string map {.c .o} $x]
    set object_cov $::b/lib/[string map {.c .cov.o} $x]
    obj $object $::s/lib/$x "-I $::s/include -I $::b/lib" $::b/lib/config.h
    obj_cov $object_cov $::s/lib/$x "-I $::s/include -I $::b/lib" $::b/lib/config.h
    lappend objs $object
    lappend objs_cov $object_cov
  }

  # buildstamp.c
  # Defines FbleBuildStamp. We have this depend on all the object files to
  # force it to rebuild any time any thing changes.
  build "$::b/lib/buildstamp.c" "$::s/buildstamp $objs" \
    "$::s/buildstamp -c FbleBuildStamp > $::b/lib/buildstamp.c"

  # buildstamp.o
  obj $::b/lib/buildstamp.o $::b/lib/buildstamp.c ""
  obj_cov $::b/lib/buildstamp.cov.o $::b/lib/buildstamp.c ""
  lappend objs $::b/lib/buildstamp.o
  lappend objs_cov $::b/lib/buildstamp.cov.o

  # libraries
  lib "$::b/lib/libfble$::lext" $objs
  lib_cov "$::b/lib/libfble.cov$::lext" $objs_cov

  foreach {x} [build_glob $::s/lib -tails "*.*"] {
    fbld_check_dc $::b/lib/$x.dc $::s/lib/$x
  }

  install $::b/lib/libfble$::lext $::config::libdir/libfble$::lext

  set ::fble_objs_cov $objs_cov
}
