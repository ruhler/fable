# Exports:
#   ::fble_objs_cov - list of .o files used in libfble.cov.
namespace eval "fble/lib" {
  set objs [list]
  set objs_cov [list]

  # Update local includes for fble/lib/parse.y here.
  # See comment in fble/lib/parse.y.
  set includes [list \
    $::s/fble/include/fble-alloc.h \
    $::s/fble/include/fble-load.h \
    $::s/fble/include/fble-loc.h \
    $::s/fble/include/fble-module-path.h \
    $::s/fble/include/fble-name.h \
    $::s/fble/include/fble-string.h \
    $::s/fble/include/fble-vector.h \
    $::s/fble/lib/expr.h \
  ]
  set report $::b/fble/lib/parse.tab.report.txt
  set tabc $::b/fble/lib/parse.tab.c
  set cmd "bison --report=all --report-file=$report -o $tabc $::s/fble/lib/parse.y"
  build "$tabc $report" "$::s/fble/lib/parse.y $includes" $cmd

  # parse.tab.o
  obj $::b/fble/lib/parse.tab.o $::b/fble/lib/parse.tab.c "-I $::s/fble/include -I $::s/fble/lib"
  obj_cov $::b/fble/lib/parse.tab.cov.o $::b/fble/lib/parse.tab.c "-I $::s/fble/include -I $::s/fble/lib"
  lappend objs $::b/fble/lib/parse.tab.o
  lappend objs_cov $::b/fble/lib/parse.tab.cov.o

  # general .o files
  lappend ::build_ninja_deps "$::s/fble/lib"
  foreach {x} [glob -tails -directory $::s/fble/lib *.c] {
    set object $::b/fble/lib/[string map {.c .o} $x]
    set object_cov $::b/fble/lib/[string map {.c .cov.o} $x]
    obj $object $::s/fble/lib/$x "-I $::s/fble/include"
    obj_cov $object_cov $::s/fble/lib/$x "-I $::s/fble/include"
    lappend objs $object
    lappend objs_cov $object_cov
  }

  lib "$::b/fble/lib/libfble.a" $objs
  lib "$::b/fble/lib/libfble.cov.a" $objs_cov

  set ::fble_objs_cov $objs_cov
}
