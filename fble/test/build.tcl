namespace eval "fble/test" {
  set cflags "-I $::s/fble/include"
  set libs "$::b/fble/test/libfbletest.a $::b/fble/lib/libfble.a"
  set libs_cov "$::b/fble/test/libfbletest.a $::b/fble/lib/libfble.cov.a"

  # libfbletest.a
  set objs [list]
  foreach {x} [list test.c mem-test.c profiles-test.c] {
    set object $::b/fble/test/[string map {.c .o} $x]
    obj $object $::s/fble/test/$x $cflags
    lappend objs $object
  }
  lib $::b/fble/test/libfbletest.a $objs

  # test binaries
  lappend ::build_ninja_deps "$::s/fble/test"
  foreach {x} [glob $::s/fble/test/fble-*.c] {
    set base [file rootname [file tail $x]]
    obj $::b/fble/test/$base.o $x $cflags
    bin $::b/fble/test/$base "$::b/fble/test/$base.o $libs" ""
    bin_cov $::b/fble/test/$base.cov "$::b/fble/test/$base.o $libs_cov" ""
  }
  install_bin $::b/fble/test/fble-test
  install_bin $::b/fble/test/fble-perf-profile

  # fble-profile-test
  test $::b/fble/test/fble-profile-test.tr $::b/fble/test/fble-profile-test \
    "$::b/fble/test/fble-profile-test > /dev/null"

  # fble-profiles-test
  test $::b/fble/test/fble-profiles-test.tr \
    "$::b/fble/test/fble-profiles-test $::s/fble/test/ProfilesTest.fble" \
    "$::b/fble/test/fble-profiles-test -I $::s/fble/test -m /ProfilesTest% > $::b/fble/test/fble-profiles-test.prof"

  # fble-compiled-profiles-test-c
  fbleobj_c $::b/fble/test/ProfilesTest.c.o $::b/fble/bin/fble-compile \
    "-c -e FbleCompiledMain --main FbleProfilesTestMain -I $::s/fble/test -m /ProfilesTest%" \
    $::s/fble/test/ProfilesTest.fble
  bin $::b/fble/test/ProfilesTest.c "$::b/fble/test/ProfilesTest.c.o $libs" ""

  test $::b/fble/test/ProfilesTest.c.tr "$::b/fble/test/ProfilesTest.c" \
    "$::b/fble/test/ProfilesTest.c > $::b/fble/test/ProfilesTest.c.prof"

  if {$::arch == "aarch64"} {
    # fble-compiled-profiles-test-aarch64
    fbleobj_aarch64 $::b/fble/test/ProfilesTest.aarch64.o $::b/fble/bin/fble-compile \
      "-c -e FbleCompiledMain --main FbleProfilesTestMain -I $::s/fble/test -m /ProfilesTest%" \
      $::s/fble/test/ProfilesTest.fble
    bin $::b/fble/test/ProfilesTest.aarch64 "$::b/fble/test/ProfilesTest.aarch64.o $libs" ""
    test $::b/fble/test/ProfilesTest.aarch64.tr "$::b/fble/test/ProfilesTest.aarch64" \
      "$::b/fble/test/ProfilesTest.aarch64 > $::b/fble/test/ProfilesTest.aarch64.prof"

    # /Fble/DebugTest%
    fbleobj_aarch64 $::b/fble/test/fble-debug-test.o $::b/fble/bin/fble-compile \
      "--main FbleTestMain -c -I $::s/fble/test -m /DebugTest%"
    bin $::b/fble/test/fble-debug-test "$::b/fble/test/fble-debug-test.o $libs" ""

    # Test that there are no dwarf warnings in the generated fble-debug-test
    # binary.
    build "$::b/fble/test/fble-debug-test.dwarf $::b/fble/test/fble-debug-test.dwarf-warnings.txt" \
      "$::b/fble/test/fble-debug-test" \
      "objdump --dwarf $::b/fble/test/fble-debug-test > $::b/fble/test/fble-debug-test.dwarf 2> $::b/fble/test/fble-debug-test.dwarf-warnings.txt"

    test $::b/fble/test/dwarf-test.tr \
      "$::b/fble/test/fble-debug-test.dwarf-warnings.txt" \
      "cmp /dev/null $::b/fble/test/fble-debug-test.dwarf-warnings.txt"

    test $::b/fble/test/fble-debug-test.tr \
      "$::b/fble/test/fble-debug-test $::s/fble/test/fble-debug-test.exp" \
      "expect $::s/fble/test/fble-debug-test.exp $::s $::b"
  }
}
