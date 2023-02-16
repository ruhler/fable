namespace eval "spec-test" {
  set spec_tests [list]
  set deps [list \
    $::s/test/spec-test.run.tcl \
    $::b/lib/libfble.a \
    $::b/bin/fble-compile.cov \
    $::b/bin/fble-disassemble.cov \
    $::b/test/fble-test.cov \
    $::b/test/fble-mem-test.cov \
    $::b/test/libfbletest.a \
  ]

  foreach dir [dirs $::s/spec ""] {
    lappend ::build_ninja_deps "$::s/spec/$dir"
    foreach {fble} [lsort [glob -tails -directory $::s/spec -nocomplain -type f $dir/*.fble]] {
      # Add quotes around module path words so we can name spec tests to match
      # spec section numbers, e.g. 2.2-Kinds.
      set mpath "/\\'[string map { "/" "\\'/\\'"} [file rootname $fble]]\\'%"
      set depfile $::b/spec/$fble.d
      build $depfile "$::b/bin/fble-deps $::s/spec/$fble" \
        "$::b/bin/fble-deps -I $::s/spec -t $depfile -m $mpath > $depfile 2> /dev/null" \
        "depfile = $depfile"

      lappend spec_tests $::b/spec/$fble.tr
      testsuite $::b/spec/$fble.tr "$deps $::s/spec/$fble $depfile" \
        "tclsh8.6 $::s/test/spec-test.run.tcl $::s $::b $fble"
    }
  }

  # Code coverage. Mark it as 'testsuite' so it gets built along with all the
  # other test cases.
  # TODO: There should be a better way to say when it gets built.
  testsuite $::b/cov/gcov.txt "$::fble_objs_cov $spec_tests" \
    "gcov $::fble_objs_cov > $::b/cov/gcov.txt && mv *.gcov $::b/cov"
}
