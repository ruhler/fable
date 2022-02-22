namespace eval "lang" {
  # fble language spec tests
  # 
  # The code for running the spec tests is split up a little bit awkwardly
  # because we want to reuse ninja to build intermediate artifacts used in the
  # test. The intermediate artifacts we need to build depend on the contents of
  # the test specification. We split it up as follows:
  # * build.tcl (this file) - reads the spec tests and uses it to generate
  #   build rules, because apparently ninja needs all the build rules to show up
  #   in build.ninja from the beginning. Does as little else as possible because
  #   build.tcl is rerun for lots of reasons. In particular, does not
  #   extract the .fble files from the test.
  # * fble/test/extract-spec-test.tcl - extracts the .fble files from a particular
  #   test. This is run at build time so we avoid re-extracting the .fble files
  #   repeatedly if the test specification hasn't changed.
  # * fble/test/run-spec-test.tcl - a helper function to execute a spec test that
  #   takes the command to run as input. The primary purpose of this is to
  #   encapsulate an otherwise complex command line to run the test and give us
  #   nice test failure messages. It reads the test spec for the test to run,
  #   but only to know what error location, if any, is expected for
  #   fble-test-*-error tests.
  set spec_tests [list]
  foreach dir [dirs langs/fble ""] {
    lappend ::build_ninja_deps "langs/fble/$dir"
    foreach {t} [lsort [glob -tails -directory langs/fble -nocomplain -type f $dir/*.tcl]] {
      set ::specroot [file rootname $t]
      set ::spectestdir $::out/langs/fble/$specroot
      set ::spectcl langs/fble/$t
      lappend ::build_ninja_deps $::spectcl

      # Returns the list of .fble files for modules used in the test, not
      # including the top level .fble file.
      proc collect_modules { dir modules } {
        set fbles [list]
        foreach m $modules {
          set name [lindex $m 0]
          set submodules [lrange $m 2 end]
          lappend fbles $dir/$name.fble
          lappend fbles {*}[collect_modules $dir/$name $submodules]
        }
        return $fbles
      }

      # Emit build rules to compile all the .fble files for the test together
      # into a compiled-test binary.
      #
      # Inputs:
      #   main - The value to use for --main option to fble-compile for the top
      #          level module.
      #   modules - Non-top level modules to include.
      proc spec-test-compile { main modules } {
        set fbles [collect_modules "" $modules]
        lappend fbles "/Nat.fble"

        set objs [list]
        foreach x $fbles {
          set path [file rootname $x]%
          set fble $::spectestdir$x

          set o [string map {.fble .o} $fble]
          lappend objs $o

          fbleobj $o $::out/fble/bin/fble-compile.cov "-c -I $::spectestdir -m $path" $::spectestdir/test.fble
        }

        fbleobj $::spectestdir/test.o $::out/fble/bin/fble-compile.cov "--main $main -c -e FbleCompiledMain -I $::spectestdir -m /test%" $::spectestdir/test.fble
        lappend objs $::spectestdir/test.o

        lappend objs $::out/fble/test/libfbletest.a
        lappend objs $::out/fble/lib/libfble.a
        bin $::spectestdir/compiled-test $objs ""

        # Run fble-disassemble here to get test coverage fble-disassemble.
        lappend lang::spec_tests $::spectestdir/test.asm.tr
        test $::spectestdir/test.asm.tr \
          "$::out/fble/bin/fble-disassemble.cov $::spectestdir/test.fble" \
          "$::out/fble/bin/fble-disassemble.cov -I $::spectestdir -m /test% > $::spectestdir/test.asm"
      }

      proc spec-test-extract {} {
        build "$::spectestdir/test.fble"  \
          "fble/test/extract-spec-test.tcl $::spectcl langs/fble/Nat.fble" \
          "tclsh fble/test/extract-spec-test.tcl $::spectcl $::spectestdir langs/fble/Nat.fble"
      }

      proc fble-test { expr args } {
        spec-test-extract
        spec-test-compile FbleTestMain $args

        # We run with --profile to get better test coverage for profiling.
        lappend lang::spec_tests $::spectestdir/test.tr
        test $::spectestdir/test.tr "fble/test/run-spec-test.tcl $::out/fble/test/fble-test.cov $::spectestdir/test.fble" \
          "tclsh fble/test/run-spec-test.tcl $::spectcl $::out/fble/test/fble-test.cov --profile /dev/null -I $::spectestdir -m /test%"

        lappend lang::spec_tests $::spectestdir/test-compiled.tr
        test $::spectestdir/test-compiled.tr \
          "fble/test/run-spec-test.tcl $::spectestdir/compiled-test" \
          "tclsh fble/test/run-spec-test.tcl $::spectcl $::spectestdir/compiled-test --profile /dev/null"
      }

      proc fble-test-compile-error { loc expr args } {
        spec-test-extract

        lappend lang::spec_tests $::spectestdir/test.tr
        test $::spectestdir/test.tr "fble/test/run-spec-test.tcl $::out/fble/test/fble-test.cov $::spectestdir/test.fble" \
          "tclsh fble/test/run-spec-test.tcl $::spectcl $::out/fble/test/fble-test.cov --compile-error -I $::spectestdir -m /test%"

        # TODO: Test that we get the desired compilation error when running the
        # compiler?
      }

      proc fble-test-runtime-error { loc expr args } {
        spec-test-extract
        spec-test-compile FbleTestMain $args

        lappend lang::spec_tests $::spectestdir/test.tr
        test $::spectestdir/test.tr "fble/test/run-spec-test.tcl $::out/fble/test/fble-test.cov $::spectestdir/test.fble" \
          "tclsh fble/test/run-spec-test.tcl $::spectcl $::out/fble/test/fble-test.cov --runtime-error -I $::spectestdir -m /test%"

        lappend lang::spec_tests $::spectestdir/test-compiled.tr
        test $::spectestdir/test-compiled.tr \
          "fble/test/run-spec-test.tcl $::spectestdir/compiled-test" \
          "tclsh fble/test/run-spec-test.tcl $::spectcl $::spectestdir/compiled-test --runtime-error"
      }

      proc fble-test-memory-constant { expr } {
        spec-test-extract
        spec-test-compile FbleMemTestMain {}

        lappend lang::spec_tests $::spectestdir/test.tr
        test $::spectestdir/test.tr "fble/test/run-spec-test.tcl $::out/fble/test/fble-mem-test.cov $::spectestdir/test.fble" \
          "tclsh fble/test/run-spec-test.tcl $::spectcl $::out/fble/test/fble-mem-test.cov -I $::spectestdir -m /test%"

        lappend lang::spec_tests $::spectestdir/test-compiled.tr
        test $::spectestdir/test-compiled.tr \
          "fble/test/run-spec-test.tcl $::spectestdir/compiled-test" \
          "tclsh fble/test/run-spec-test.tcl $::spectcl $::spectestdir/compiled-test"
      }

      proc fble-test-memory-growth { expr } {
        spec-test-extract
        spec-test-compile FbleMemTestMain {}

        lappend lang::spec_tests $::spectestdir/test.tr
        test $::spectestdir/test.tr "fble/test/run-spec-test.tcl $::out/fble/test/fble-mem-test.cov $::spectestdir/test.fble" \
          "tclsh fble/test/run-spec-test.tcl $::spectcl $::out/fble/test/fble-mem-test.cov --growth -I $::spectestdir -m /test%"

        lappend lang::spec_tests $::spectestdir/test-compiled.tr
        test $::spectestdir/test-compiled.tr \
          "fble/test/run-spec-test.tcl $::spectestdir/compiled-test" \
          "tclsh fble/test/run-spec-test.tcl $::spectcl $::spectestdir/compiled-test --growth"
      }
      source $::spectcl
    }
  }

  # Code coverage from spec tests.
  build $::out/cov/gcov.txt "$::fble_objs_cov $lang::spec_tests" \
    "gcov $::fble_objs_cov > $::out/cov/gcov.txt && mv *.gcov $::out/cov"
}
