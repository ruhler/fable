# Exports:
#   ::fble_objs_cov - list of .o files used in libfble.cov.
namespace eval "lib" {
  dist_s $::s/lib/build.tcl
  dist_s $::s/lib/parse.y
  foreach {x} [build_glob $::s/lib *.h *.c] {
    dist_s $x
  }

  set objs [list]
  set objs_cov [list]

  set report $::b/lib/parse.tab.report.txt
  set cmd "bison --report=all --report-file=$report -o $::d/lib/parse.tab.c $::s/lib/parse.y"
  build "$::d/lib/parse.tab.c $report" "$::s/lib/parse.y" $cmd
  dist_d $::d/lib/parse.tab.c

  # parse.tab.o
  obj $::b/lib/parse.tab.o $::d/lib/parse.tab.c "-I $::s/include -I $::s/lib"
  obj_cov $::b/lib/parse.tab.cov.o $::d/lib/parse.tab.c "-I $::s/include -I $::s/lib"
  lappend objs $::b/lib/parse.tab.o
  lappend objs_cov $::b/lib/parse.tab.cov.o

  # general .o files
  foreach {x} [build_glob $::s/lib -tails *.c] {
    set object $::b/lib/[string map {.c .o} $x]
    set object_cov $::b/lib/[string map {.c .cov.o} $x]
    obj $object $::s/lib/$x "-I $::s/include"
    obj_cov $object_cov $::s/lib/$x "-I $::s/include"
    lappend objs $object
    lappend objs_cov $object_cov
  }

  lib "$::b/lib/libfble.a" $objs
  lib "$::b/lib/libfble.cov.a" $objs_cov

  install $::b/lib/libfble.a $::config::libdir/libfble.a

  set ::fble_objs_cov $objs_cov
}
