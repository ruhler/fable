namespace eval "pkgs/core" {
  fbld_header_usage $::b/pkgs/core/fble-stdio.usage.h $::s/pkgs/core/fble-stdio.fbld \
    fbldUsageHelpText

  # .c library files.
  set objs [list]
  foreach {x} { char.fble int.fble stdio.fble string.fble } {
    lappend objs $::b/pkgs/core/$x.o
    obj $::b/pkgs/core/$x.o $::s/pkgs/core/$x.c \
      "-I $::s/include -I $::s/pkgs/core -I $::b/pkgs/core" \
      $::b/pkgs/core/fble-stdio.usage.h
  }
  pkg core [list] "" $objs

  # Check doc comments
  foreach {x} [build_glob $::s/pkgs/core -tails "*.h" "*.c"] {
    fbld_check_dc $::b/pkgs/core/$x.dc $::s/pkgs/core/$x
  }

  # fble-stdio program.
  fbld_man_usage $::b/pkgs/core/fble-stdio.1 $::s/pkgs/core/fble-stdio.fbld
  install $::b/pkgs/core/fble-stdio.1 $::config::mandir/man1/fble-stdio.1
  obj $::b/pkgs/core/fble-stdio.o $::s/pkgs/core/fble-stdio.c \
    "-I $::s/include -I $::s/pkgs/core"
  bin $::b/pkgs/core/fble-stdio \
    "$::b/pkgs/core/fble-stdio.o" \
    "$::b/pkgs/core/libfble-core$::lext $::b/lib/libfble$::lext" ""
  install $::b/pkgs/core/fble-stdio $::config::bindir/fble-stdio

  # Build an fble-stdio compiled binary.
  #
  # Inputs:
  #   target - the file to build.
  #   path - the module path to use as Stdio@ main.
  #   libs - additional fble packages this depends on, not including core,
  #          without the fble- prefix.
  #   lflags - additional linker flags to use when creating the binary.
  proc ::stdio { target path libs lflags} {
    set objs $target.o
    set nlibs ""
    foreach lib [lreverse $libs] {
      append nlibs " $::b/pkgs/$lib/libfble-$lib$::lext"
    }
    append nlibs " $::b/pkgs/core/libfble-core$::lext $::b/lib/libfble$::lext"

    fbleobj $target.o $::b/bin/fble-compile \
      "--main FbleStdioMain -m $path"
    bin $target $objs $nlibs $lflags
  }

  # /Core/Stdio/Cat% interpreted test.
  build $::b/pkgs/core/Core/Stdio/fble-cat.out \
    "$::b/pkgs/core/fble-stdio $::b/pkgs/core/Core/Stdio/Cat.fble.d" \
    "$::b/pkgs/core/fble-stdio -I $::s/pkgs/core -m /Core/Stdio/Cat% < $::s/README.fbld > $::b/pkgs/core/Core/Stdio/fble-cat.out"
  test $::b/pkgs/core/Core/Stdio/fble-cat.tr \
    "$::b/pkgs/core/Core/Stdio/fble-cat.out" \
    "diff --strip-trailing-cr $::b/pkgs/core/Core/Stdio/fble-cat.out $::s/README.fbld"

  # /Core/Stdio/Cat% interpreted test 2.
  build $::b/pkgs/core/Core/Stdio/fble-cat.2.out \
    "$::b/pkgs/core/fble-stdio $::b/pkgs/core/Core/Stdio/Cat.fble.d" \
    "$::b/pkgs/core/fble-stdio -I $::s/pkgs/core -m /Core/Stdio/Cat% $::s/README.fbld > $::b/pkgs/core/Core/Stdio/fble-cat.2.out"
  test $::b/pkgs/core/Core/Stdio/fble-cat.2.tr \
    "$::b/pkgs/core/Core/Stdio/fble-cat.2.out" \
    "diff --strip-trailing-cr $::b/pkgs/core/Core/Stdio/fble-cat.2.out $::s/README.fbld"

  # /Core/Stdio/Cat% compiled.
  stdio $::b/pkgs/core/fble-cat "/Core/Stdio/Cat%" "" ""
  install $::b/pkgs/core/fble-cat $::config::bindir/fble-cat
  fbld_man_usage $::b/pkgs/core/fble-cat.1 $::s/pkgs/core/fble-cat.fbld
  install $::b/pkgs/core/fble-cat.1 $::config::mandir/man1/fble-cat.1

  # /Core/Stdio/FastCat% compiled.
  stdio $::b/pkgs/core/fble-fast-cat "/Core/Stdio/FastCat%" "" ""
  install $::b/pkgs/core/fble-fast-cat $::config::bindir/fble-fast-cat

  # /Core/Stdio/HelloWorld% interpreted test.
  test $::b/pkgs/core/Core/Stdio/fble-stdio.tr "$::b/pkgs/core/fble-stdio $::b/pkgs/core/Core/Stdio/HelloWorld.fble.d" \
    "$::b/pkgs/core/fble-stdio -I $::s/pkgs/core -m /Core/Stdio/HelloWorld% | grep hello"

  # /Core/Stdio/HelloWorld% compiled test.
  stdio $::b/pkgs/core/Core/Stdio/fble-stdio-test "/Core/Stdio/HelloWorld%" "" ""
  test $::b/pkgs/core/Core/Stdio/fble-stdio-test.out \
    $::b/pkgs/core/Core/Stdio/fble-stdio-test \
    "$::b/pkgs/core/Core/Stdio/fble-stdio-test > $::b/pkgs/core/Core/Stdio/fble-stdio-test.out"
  test $::b/pkgs/core/Core/Stdio/fble-stdio-test.tr $::b/pkgs/core/Core/Stdio/fble-stdio-test.out \
    "grep hello $::b/pkgs/core/Core/Stdio/fble-stdio-test.out"

  # Core/Tests interpreted
  testsuite $::b/pkgs/core/Core/tests.tr "$::b/pkgs/core/fble-stdio $::b/pkgs/core/Core/Tests.fble.d" \
    "$::b/pkgs/core/fble-stdio -I $::s/pkgs/core -m /Core/Tests% --prefix Interpreted."

  # Core/Tests compiled
  stdio $::b/pkgs/core/Core/core-tests "/Core/Tests%" "" ""
  testsuite $::b/pkgs/core/Core/core-tests.tr $::b/pkgs/core/Core/core-tests \
    "$::b/pkgs/core/Core/core-tests --prefix Compiled."
}
