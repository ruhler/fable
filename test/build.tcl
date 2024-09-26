namespace eval "test" {
  set cflags "-I $::s/include -I $::b/test"
  set libs "$::b/test/libfbletest.a $::b/lib/libfble.a"
  set libs_cov "$::b/test/libfbletest.a $::b/lib/libfble.cov.a"

  set lib_sources {
    test.c
    mem-test.c
    profiles-test.c
  }

  set bin_sources {
    fble-mem-test.c
    fble-profiles-test.c
    fble-profile-test.c
    fble-test.c
  }

  # libfbletest.a
  set objs [list]
  foreach {x} $lib_sources {
    set base [file rootname [file tail $x]]
    fbld_header_usage $::b/test/fble-$base.usage.h $::s/test/fble-$base.fbld \
      fbldUsageHelpText
    fbld_help_usage $::b/test/fble-$base.usage.txt $::s/test/fble-$base.fbld
    fbld_man_usage $::b/test/fble-$base.1 $::s/test/fble-$base.fbld
    obj $::b/test/$base.o $::s/test/$base.c $cflags $::b/test/fble-$base.usage.h
    lappend objs $::b/test/$base.o
  }
  lib $::b/test/libfbletest.a $objs

  # test binaries
  foreach {x} $bin_sources {
    set base [file rootname [file tail $x]]
    obj $::b/test/$base.o $::s/test/$x $cflags
    bin $::b/test/$base "$::b/test/$base.o $libs" ""
    bin_cov $::b/test/$base.cov "$::b/test/$base.o $libs_cov" ""
  }

  foreach {x} [list fble-test fble-mem-test] {
    install $::b/test/$x.usage.txt $::config::docdir/fble/$x.usage.txt
    install $::b/test/$x.1 $::config::mandir/man1/$x.1
    install $::b/test/$x $::config::bindir/$x
  }

  # Check doc comments
  foreach {x} [build_glob $::s/test -tails "*.h" "*.c"] {
    fbld_check_dc $::b/test/$x.dc $::s/test/$x
  }

  # fble-profile-test
  test $::b/test/fble-profile-test.tr $::b/test/fble-profile-test \
    "$::b/test/fble-profile-test > /dev/null"

  # fble-profiles-test
  test $::b/test/fble-profiles-test.tr \
    "$::b/test/fble-profiles-test $::s/test/ProfilesTest.fble" \
    "$::b/test/fble-profiles-test -I $::s/test -m /ProfilesTest% > $::b/test/fble-profiles-test.prof"

  # fble-compiled-profiles-test-c
  fbleobj_c $::b/test/ProfilesTest.c.o $::b/bin/fble-compile \
    "-c -e FbleCompiledMain --main FbleProfilesTestMain -I $::s/test -m /ProfilesTest%" \
    $::s/test/ProfilesTest.fble
  bin $::b/test/ProfilesTest.c "$::b/test/ProfilesTest.c.o $libs" ""

  test $::b/test/ProfilesTest.c.tr "$::b/test/ProfilesTest.c" \
    "$::b/test/ProfilesTest.c > $::b/test/ProfilesTest.c.prof"

  test $::b/tests/fble-profiles-test.usage.txt.tr \
    $::b/test/fble-profiles-test.usage.txt \
    "touch $::b/tests/fble-profiles-test.usage.txt"

  if {$::arch == "aarch64"} {
    # fble-compiled-profiles-test-aarch64
    fbleobj_aarch64 $::b/test/ProfilesTest.aarch64.o $::b/bin/fble-compile \
      "-c -e FbleCompiledMain --main FbleProfilesTestMain -I $::s/test -m /ProfilesTest%" \
      $::s/test/ProfilesTest.fble
    bin $::b/test/ProfilesTest.aarch64 "$::b/test/ProfilesTest.aarch64.o $libs" ""
    test $::b/test/ProfilesTest.aarch64.tr "$::b/test/ProfilesTest.aarch64" \
      "$::b/test/ProfilesTest.aarch64 > $::b/test/ProfilesTest.aarch64.prof"

    # /Fble/DebugTest%
    fbleobj_aarch64 $::b/test/fble-debug-test.o $::b/bin/fble-compile \
      "--main FbleTestMain -c -I $::s/test -m /DebugTest%"
    bin $::b/test/fble-debug-test "$::b/test/fble-debug-test.o $libs" ""

    # Test that there are no dwarf warnings in the generated fble-debug-test
    # binary.
    build "$::b/test/fble-debug-test.dwarf $::b/test/fble-debug-test.dwarf-warnings.txt" \
      "$::b/test/fble-debug-test" \
      "objdump --dwarf $::b/test/fble-debug-test > $::b/test/fble-debug-test.dwarf 2> $::b/test/fble-debug-test.dwarf-warnings.txt"

    test $::b/test/dwarf-test.tr \
      "$::b/test/fble-debug-test.dwarf-warnings.txt" \
      "cmp /dev/null $::b/test/fble-debug-test.dwarf-warnings.txt"

    test $::b/test/fble-debug-test.tr \
      "$::b/test/fble-debug-test $::s/test/fble-debug-test.exp" \
      "expect $::s/test/fble-debug-test.exp $::s $::b"
  }
}
