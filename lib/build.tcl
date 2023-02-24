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
  set cmd "bison --report=all --report-file=$report -o $::b/lib/parse.tab.c $::s/lib/parse.y"
  build "$::b/lib/parse.tab.c $report" "$::s/lib/parse.y" $cmd

  # parse.tab.o
  obj $::b/lib/parse.tab.o $::b/lib/parse.tab.c "-I $::s/include -I $::s/lib"
  obj_cov $::b/lib/parse.tab.cov.o $::b/lib/parse.tab.c "-I $::s/include -I $::s/lib"
  lappend objs $::b/lib/parse.tab.o
  lappend objs_cov $::b/lib/parse.tab.cov.o

  # load.path.c
  build "$::b/lib/load.path.c" "" \
    "echo \"const char* FbleDefaultPackagePath = \\\"$::config::datadir/fble\\\";\" > $::b/lib/load.path.c"

  # load.path.o
  obj $::b/lib/load.path.o $::b/lib/load.path.c ""
  obj_cov $::b/lib/load.path.cov.o $::b/lib/load.path.c ""
  lappend objs $::b/lib/load.path.o
  lappend objs_cov $::b/lib/load.path.cov.o

  # general .o files
  foreach {x} $sources {
    set object $::b/lib/[string map {.c .o} $x]
    set object_cov $::b/lib/[string map {.c .cov.o} $x]
    obj $object $::s/lib/$x "-I $::s/include"
    obj_cov $object_cov $::s/lib/$x "-I $::s/include"
    lappend objs $object
    lappend objs_cov $object_cov
  }

  # buildstamp.c
  # Defines FbleBuildStamp. We have this depend on all the object files to
  # force it to rebuild any time any thing changes.
  build "$::b/lib/buildstamp.c" "$::s/buildstamp $objs" \
    "$::s/buildstamp FbleBuildStamp > $::b/lib/buildstamp.c"

  # buildstamp.o
  obj $::b/lib/buildstamp.o $::b/lib/buildstamp.c ""
  obj_cov $::b/lib/buildstamp.cov.o $::b/lib/buildstamp.c ""
  lappend objs $::b/lib/buildstamp.o
  lappend objs_cov $::b/lib/buildstamp.cov.o

  # libraries
  lib "$::b/lib/libfble.a" $objs
  lib "$::b/lib/libfble.cov.a" $objs_cov

  install $::b/lib/libfble.a $::config::libdir/libfble.a

  set ::fble_objs_cov $objs_cov
}
