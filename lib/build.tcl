# Exports:
#   ::fble_objs_cov - list of .o files used in libfble.cov.
namespace eval "lib" {
  set objs [list]
  set objs_cov [list]

  # Update local includes for lib/parse.y here.
  # See comment in lib/parse.y.
  set includes [list \
    $::s/include/fble/fble-alloc.h \
    $::s/include/fble/fble-load.h \
    $::s/include/fble/fble-loc.h \
    $::s/include/fble/fble-module-path.h \
    $::s/include/fble/fble-name.h \
    $::s/include/fble/fble-string.h \
    $::s/include/fble/fble-vector.h \
    $::s/lib/expr.h \
  ]
  set report $::b/lib/parse.tab.report.txt
  set tabc $::b/lib/parse.tab.c
  set cmd "bison --report=all --report-file=$report -o $tabc $::s/lib/parse.y"
  build "$tabc $report" "$::s/lib/parse.y $includes" $cmd

  # parse.tab.o
  obj $::b/lib/parse.tab.o $::b/lib/parse.tab.c "-I $::s/include -I $::s/lib"
  obj_cov $::b/lib/parse.tab.cov.o $::b/lib/parse.tab.c "-I $::s/include -I $::s/lib"
  lappend objs $::b/lib/parse.tab.o
  lappend objs_cov $::b/lib/parse.tab.cov.o

  # general .o files
  lappend ::build_ninja_deps "$::s/lib"
  foreach {x} [glob -tails -directory $::s/lib *.c] {
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
