namespace eval "fble/test" {
  set cflags "-I fble/include"
  set libs "$::out/fble/test/libfbletest.a $::out/fble/lib/libfble.a"
  set libs_cov "$::out/fble/test/libfbletest.a $::out/fble/lib/libfble.cov.a"

  # libfbletest.a
  set objs [list]
  foreach {x} [list test.c mem-test.c profiles-test.c] {
    set object $::out/fble/test/[string map {.c .o} $x]
    obj $object fble/test/$x $cflags
    lappend objs $object
  }
  lib $::out/fble/test/libfbletest.a $objs

  # test binaries
  lappend ::build_ninja_deps "fble/test"
  foreach {x} [glob fble/test/fble-*.c] {
    set base [file rootname [file tail $x]]
    obj $::out/fble/test/$base.o $x $cflags
    bin $::out/fble/test/$base "$::out/fble/test/$base.o $libs" ""
    bin_cov $::out/fble/test/$base.cov "$::out/fble/test/$base.o $libs_cov" ""
  }

  # fble-profile-test
  test $::out/fble/test/fble-profile-test.tr $::out/fble/test/fble-profile-test \
    "$::out/fble/test/fble-profile-test > /dev/null"

  # fble-profiles-test
  test $::out/fble/test/fble-profiles-test.tr \
    "$::out/fble/test/fble-profiles-test fble/test/ProfilesTest.fble" \
    "$::out/fble/test/fble-profiles-test -I fble/test -m /ProfilesTest% > $::out/fble/test/fble-profiles-test.prof"

  if {$::arch == "aarch64"} {
    # fble-compiled-profiles-test
    fbleobj $::out/fble/test/ProfilesTest.o $::out/fble/bin/fble-compile \
      "-c -e FbleCompiledMain --main FbleProfilesTestMain -I fble/test -m /ProfilesTest%" \
      fble/test/ProfilesTest.fble
    bin $::out/fble/test/ProfilesTest "$::out/fble/test/ProfilesTest.o $libs" ""
    test $::out/fble/test/ProfilesTest.tr "$::out/fble/test/ProfilesTest" \
      "$::out/fble/test/ProfilesTest > $::out/fble/test/ProfilesTest.prof"

    # /Fble/DebugTest%
    build $::out/fble/test/fble-debug-test.s $::out/fble/bin/fble-compile \
      "$::out/fble/bin/fble-compile --main FbleTestMain -c -I fble/test -m /DebugTest% > $::out/fble/test/fble-debug-test.s"
    asm $::out/fble/test/fble-debug-test.o $::out/fble/test/fble-debug-test.s
    bin $::out/fble/test/fble-debug-test "$::out/fble/test/fble-debug-test.o $libs" ""

    # Test that there are no dwarf warnings in the generated fble-debug-test
    # binary.
    build "$::out/fble/test/fble-debug-test.dwarf $::out/fble/test/fble-debug-test.dwarf-warnings.txt" \
      "$::out/fble/test/fble-debug-test" \
      "objdump --dwarf $::out/fble/test/fble-debug-test > $::out/fble/test/fble-debug-test.dwarf 2> $::out/fble/test/fble-debug-test.dwarf-warnings.txt"

    test $::out/fble/test/dwarf-test.tr \
      "$::out/fble/test/fble-debug-test.dwarf-warnings.txt" \
      "cmp /dev/null $::out/fble/test/fble-debug-test.dwarf-warnings.txt"

    test $::out/fble/test/fble-debug-test.tr \
      "$::out/fble/test/fble-debug-test fble/test/fble-debug-test.exp" \
      "expect fble/test/fble-debug-test.exp"
  }
}
