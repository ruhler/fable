namespace eval "spec-test" {
  set spec_tests [list]
  set deps [list \
    $::s/fble/test/spec-test.run.tcl \
    $::b/fble/lib/libfble.a \
    $::b/fble/bin/fble-disassemble.cov \
    $::b/fble/test/fble-test.cov \
    $::b/fble/test/fble-mem-test.cov \
    $::b/fble/test/libfbletest.a \
  ]

  foreach dir [dirs $::s/spec ""] {
    lappend ::build_ninja_deps "$::s/spec/$dir"
    foreach {fble} [lsort [glob -tails -directory $::s/spec -nocomplain -type f $dir/*.fble]] {
      # Add quotes around module path words so we can name spec tests to match
      # spec section numbers, e.g. 2.2-Kinds.
      set mpath "/\\'[string map { "/" "\\'/\\'"} [file rootname $fble]]\\'%"
      set depfile $::b/spec/$fble.d
      build $depfile "$::b/fble/bin/fble-deps $::s/spec/$fble" \
        "$::b/fble/bin/fble-deps -I $::s/spec -t $depfile -m $mpath > $depfile 2> /dev/null" \
        "depfile = $depfile"

      lappend spec_tests $::b/spec/$fble.tr
      testsuite $::b/spec/$fble.tr "$deps $::s/spec/$fble $depfile" \
        "tclsh $::s/fble/test/spec-test.run.tcl $::s $::b $fble"
    }
  }

  build $::b/cov/gcov.txt "$::fble_objs_cov $spec_tests" \
    "gcov $::fble_objs_cov > $::b/cov/gcov.txt && mv *.gcov $::b/cov"
}
