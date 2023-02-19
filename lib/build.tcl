# Exports:
#   ::fble_objs_cov - list of .o files used in libfble.cov.
namespace eval "lib" {
  set headers {
    code.h
    expr.h
    heap.h
    interpret.h
    kind.h
    tc.h
    typecheck.h
    type.h
    unreachable.h
    value.h
    var.h
  }

  set sources {
    alloc.c
    arg-parse.c
    code.c
    compile.c
    execute.c
    expr.c
    generate-aarch64.c
    generate-c.c
    heap.c
    interpret.c
    kind.c
    link.c
    load.c
    loc.c
    module-path.c
    name.c
    profile.c
    string.c
    tc.c
    type.c
    typecheck.c
    value.c
    vector.c
  }

  # files for distribution
  dist_s $::s/lib/build.tcl
  dist_s $::s/lib/parse.y
  foreach {x} $sources { dist_s $::s/lib/$x }
  foreach {x} $headers { dist_s $::s/lib/$x }

  set objs [list]
  set objs_cov [list]

  # parse.tab.c
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
  foreach {x} $sources {
    set object $::b/lib/[string map {.c .o} $x]
    set object_cov $::b/lib/[string map {.c .cov.o} $x]
    obj $object $::s/lib/$x "-I $::s/include"
    obj_cov $object_cov $::s/lib/$x "-I $::s/include"
    lappend objs $object
    lappend objs_cov $object_cov
  }

  # libraries
  lib "$::b/lib/libfble.a" $objs
  lib "$::b/lib/libfble.cov.a" $objs_cov

  install $::b/lib/libfble.a $::config::libdir/libfble.a

  set ::fble_objs_cov $objs_cov
}
