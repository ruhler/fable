# Exports:
#   ::fble_objs_cov - list of .o files used in libfble.cov.
namespace eval "fble/lib" {
  set objs [list]
  set objs_cov [list]

  # Update local includes for fble/lib/parse.y here.
  # See comment in fble/lib/parse.y.
  set includes {
    fble/include/fble-alloc.h
    fble/include/fble-load.h
    fble/include/fble-loc.h
    fble/include/fble-module-path.h
    fble/include/fble-name.h
    fble/include/fble-string.h
    fble/include/fble-vector.h
    fble/lib/expr.h
  }
  set report $::out/fble/lib/parse.tab.report.txt
  set tabc $::out/fble/lib/parse.tab.c
  set cmd "bison --report=all --report-file=$report -o $tabc fble/lib/parse.y"
  build "$tabc $report" "fble/lib/parse.y $includes" $cmd

  # parse.tab.o
  obj $::out/fble/lib/parse.tab.o $::out/fble/lib/parse.tab.c "-I fble/include -I fble/lib"
  obj_cov $::out/fble/lib/parse.tab.cov.o $::out/fble/lib/parse.tab.c "-I fble/include -I fble/lib"
  lappend objs $::out/fble/lib/parse.tab.o
  lappend objs_cov $::out/fble/lib/parse.tab.cov.o

  # general .o files
  lappend ::build_ninja_deps "fble/lib"
  foreach {x} [glob -tails -directory fble/lib *.c] {
    set object $::out/fble/lib/[string map {.c .o} $x]
    set object_cov $::out/fble/lib/[string map {.c .cov.o} $x]
    obj $object fble/lib/$x "-I fble/include"
    obj_cov $object_cov fble/lib/$x "-I fble/include"
    lappend objs $object
    lappend objs_cov $object_cov
  }

  lib "$::out/fble/lib/libfble.a" $objs
  lib "$::out/fble/lib/libfble.cov.a" $objs_cov

  set ::fble_objs_cov $objs_cov
}
