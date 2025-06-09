namespace eval "test" {
  set cflags "-I $::s/include -I $::b/test"
  set libs "$::b/test/libfbletest$::lext"
  set libs_cov "$::b/test/libfbletest$::lext"

  set lib_sources {
    mem-test.c
    profiles-test.c
    test.c
  }

  set bin_sources {
    fble-mem-test.c
    fble-profiles-test.c
    fble-profile-test.c
    fble-pprof-test.c
    fble-test.c
  }

  # libfbletest
  set objs [list]
  foreach {x} $lib_sources {
    set base [file rootname [file tail $x]]
    fbld_header_usage $::b/test/fble-$base.usage.h $::s/test/fble-$base.fbld \
      fbldUsageHelpText
    fbld_man_usage $::b/test/fble-$base.1 $::s/test/fble-$base.fbld
    obj $::b/test/$base.o $::s/test/$base.c $cflags $::b/test/fble-$base.usage.h
    lappend objs $::b/test/$base.o
  }
  obj $::b/test/builtin.o $::s/test/builtin.c $cflags
  lappend objs $::b/test/builtin.o
  lib $::b/test/libfbletest$::lext $objs

  # test binaries
  foreach {x} $bin_sources {
    set base [file rootname [file tail $x]]
    obj $::b/test/$base.o $::s/test/$x $cflags
    bin $::b/test/$base "$::b/test/$base.o $libs" "$::b/lib/libfble$::lext" ""
    bin_cov $::b/test/$base.cov "$::b/test/$base.o $libs_cov" "$::b/lib/libfble.cov$::lext" ""
  }

  foreach {x} [list fble-test fble-mem-test] {
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
    "$::b/test/fble-profiles-test --profile $::b/test/fble-profiles-test.prof -I $::s/test -m /ProfilesTest% > $::b/test/fble-profiles-test.prof"

  # fble-pprof-test
  build $::b/test/fble-pprof-test.got \
    "$::b/test/fble-pprof-test" \
    "$::b/test/fble-pprof-test > $::b/test/fble-pprof-test.got"
  test $::b/test/fble-pprof-test.tr \
    "$::s/test/fble-pprof-test.want $::b/test/fble-pprof-test.got" \
    "cmp $::s/test/fble-pprof-test.want $::b/test/fble-pprof-test.got"

  # fble-compiled-profiles-test-c
  fbleobj_c $::b/test/ProfilesTest.c.o $::b/bin/fble-compile \
    "-c -e FbleCompiledMain --main FbleProfilesTestMain -I $::s/test -m /ProfilesTest%"
  bin $::b/test/ProfilesTest.c "$::b/test/ProfilesTest.c.o $libs" "$::b/lib/libfble$::lext" ""

  test $::b/test/ProfilesTest.c.tr "$::b/test/ProfilesTest.c" \
    "$::b/test/ProfilesTest.c --profile $::b/test/ProfilesTest.c.prof > $::b/test/ProfilesTest.c.prof"

  if {$::arch == "aarch64"} {
    # fble-compiled-profiles-test-aarch64
    fbleobj_aarch64 $::b/test/ProfilesTest.aarch64.o $::b/bin/fble-compile \
      "-c -e FbleCompiledMain --main FbleProfilesTestMain -I $::s/test -m /ProfilesTest%"
    bin $::b/test/ProfilesTest.aarch64 "$::b/test/ProfilesTest.aarch64.o $libs" "$::b/lib/libfble$::lext" ""
    test $::b/test/ProfilesTest.aarch64.tr "$::b/test/ProfilesTest.aarch64" \
      "$::b/test/ProfilesTest.aarch64 --profile $::b/test/ProfilesTest.aarch64.prof > $::b/test/ProfilesTest.aarch64.prof"

    # /Fble/DebugTest%
    fbleobj_aarch64 $::b/test/fble-debug-test.o $::b/bin/fble-compile \
      "--main FbleTestMain -c -I $::s/test -m /DebugTest%"
    bin $::b/test/fble-debug-test "$::b/test/fble-debug-test.o $libs" "$::b/lib/libfble$::lext" ""

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
