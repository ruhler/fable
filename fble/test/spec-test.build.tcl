namespace eval "spec-test" {
  set spec_tests [list]
  set deps {
    fble/test/spec-test.run.tcl
    out/fble/lib/libfble.a
    out/fble/bin/fble-disassemble.cov
    out/fble/test/fble-test.cov
    out/fble/test/fble-mem-test.cov
    out/fble/test/libfbletest.a
  }

  foreach dir [dirs spec ""] {
    lappend ::build_ninja_deps "spec/$dir"
    foreach {fble} [lsort [glob -tails -directory spec -nocomplain -type f $dir/*.fble]] {
      set mpath "/[file rootname $fble]%"
      set depfile $::out/spec/$fble.d
      build $depfile "$::out/fble/bin/fble-deps spec/$fble" \
        "$::out/fble/bin/fble-deps -I spec -t $depfile -m $mpath > $depfile 2> /dev/null" \
        "depfile = $depfile"

      lappend spec-test::spec_tests $::out/spec/$fble.tr
      test $::out/spec/$fble.tr "$deps spec/$fble $depfile" \
        "tclsh fble/test/spec-test.run.tcl $fble"
    }
  }

  # TODO: Code coverage, once we remove code coverage from langs/build.tcl.
  #build $::out/cov/gcov.txt "$::fble_objs_cov $spec-test::spec_tests" \
    "gcov $::fble_objs_cov > $::out/cov/gcov.txt && mv *.gcov $::out/cov"
}
