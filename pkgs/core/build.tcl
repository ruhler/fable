namespace eval "pkgs/core" {
  # .c library files.
  set objs [list]
  foreach {x} { char.fble int.fble stdio.fble string.fble } {
    lappend objs $::b/pkgs/core/$x.o
    obj $::b/pkgs/core/$x.o $::s/pkgs/core/$x.c "-I $::s/include -I $::s/pkgs/core"
  }
  pkg core [list] $objs

  # fble-stdio program.
  obj $::b/pkgs/core/fble-stdio.o $::s/pkgs/core/fble-stdio.c \
    "-I $::s/include -I $::s/pkgs/core"
  bin $::b/pkgs/core/fble-stdio \
    "$::b/pkgs/core/fble-stdio.o $::b/pkgs/core/libfble-core.a $::b/lib/libfble.a" ""
  install_bin $::b/pkgs/core/fble-stdio

  # Build an fble-stdio compiled binary.
  #
  # Inputs:
  #   target - the file to build.
  #   path - the module path to use as Stdio@ main.
  #   libs - additional fble packages this depends on, not including core,
  #          without the fble- prefix.
  proc ::stdio { target path libs} {
    set objs $target.o
    foreach lib $libs {
      append objs " $::b/pkgs/$lib/libfble-$lib.a"
    }
    append objs " $::b/pkgs/core/libfble-core.a"
    append objs " $::b/lib/libfble.a"

    fbleobj $target.o $::b/bin/fble-compile \
      "--main FbleStdioMain -m $path"
    bin $target $objs ""
  }

  # /Core/Stdio/Cat% interpreted test.
  build $::b/pkgs/core/Core/Stdio/fble-cat.out \
    "$::b/pkgs/core/fble-stdio $::b/pkgs/core/Core/Stdio/Cat.fble.d" \
    "$::b/pkgs/core/fble-stdio -I $::s/pkgs/core -m /Core/Stdio/Cat% < $::s/README.md > $::b/pkgs/core/Core/Stdio/fble-cat.out"
  test $::b/pkgs/core/Core/Stdio/fble-cat.tr \
    "$::b/pkgs/core/Core/Stdio/fble-cat.out" \
    "cmp $::b/pkgs/core/Core/Stdio/fble-cat.out $::s/README.md"

  # /Core/Stdio/Test% interpreted test.
  test $::b/pkgs/core/Core/Stdio/fble-stdio.tr "$::b/pkgs/core/fble-stdio $::b/pkgs/core/Core/Stdio/Test.fble.d" \
    "$::b/pkgs/core/fble-stdio -I $::s/pkgs/core -m /Core/Stdio/Test% | grep PASSED"

  # /Core/Stdio/Test% compiled test.
  stdio $::b/pkgs/core/Core/Stdio/fble-stdio-test "/Core/Stdio/Test%" ""
  test $::b/pkgs/core/Core/Stdio/fble-stdio-test.out \
    $::b/pkgs/core/Core/Stdio/fble-stdio-test \
    "$::b/pkgs/core/Core/Stdio/fble-stdio-test > $::b/pkgs/core/Core/Stdio/fble-stdio-test.out"
  test $::b/pkgs/core/Core/Stdio/fble-stdio-test.tr $::b/pkgs/core/Core/Stdio/fble-stdio-test.out \
    "grep PASSED $::b/pkgs/core/Core/Stdio/fble-stdio-test.out"

  # Core/Tests interpreted
  testsuite $::b/pkgs/core/Core/tests.tr "$::b/pkgs/core/fble-stdio $::b/pkgs/core/Core/Tests.fble.d" \
    "$::b/pkgs/core/fble-stdio -I $::s/pkgs/core -m /Core/Tests% --prefix Interpreted."

  # Core/Tests compiled
  stdio $::b/pkgs/core/Core/core-tests "/Core/Tests%" ""
  testsuite $::b/pkgs/core/Core/core-tests.tr $::b/pkgs/core/Core/core-tests \
    "$::b/pkgs/core/Core/core-tests --prefix Compiled."
}
