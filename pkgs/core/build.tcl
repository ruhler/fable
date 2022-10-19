namespace eval "pkgs/core" {
  # .c library files.
  set objs [list]
  foreach {x} { char.fble int.fble stdio.fble string.fble } {
    lappend objs $::out/pkgs/core/$x.o
    obj $::out/pkgs/core/$x.o pkgs/core/$x.c "-I fble/include -I pkgs/core"
  }
  pkg core [list] $objs

  # fble-stdio program.
  obj $::out/pkgs/core/fble-stdio.o pkgs/core/fble-stdio.c \
    "-I fble/include -I pkgs/core"
  bin $::out/pkgs/core/fble-stdio \
    "$::out/pkgs/core/fble-stdio.o $::out/pkgs/core/libfble-core.a $::out/fble/lib/libfble.a" ""

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
      append objs " $::out/pkgs/$lib/libfble-$lib.a"
    }
    append objs " $::out/pkgs/core/libfble-core.a"
    append objs " $::out/fble/lib/libfble.a"

    fbleobj $target.o $::out/fble/bin/fble-compile \
      "--main FbleStdioMain -m $path"
    bin $target $objs ""
  }

  # /Core/Stdio/Cat% interpreted test.
  build $::out/pkgs/core/Core/Stdio/fble-cat.out \
    "$::out/pkgs/core/fble-stdio $::out/pkgs/core/Core/Stdio/Cat.fble.d" \
    "$::out/pkgs/core/fble-stdio -I pkgs/core -m /Core/Stdio/Cat% < README.md > $::out/pkgs/core/Core/Stdio/fble-cat.out"
  test $::out/pkgs/core/Core/Stdio/fble-cat.tr \
    "$::out/pkgs/core/Core/Stdio/fble-cat.out" \
    "cmp $::out/pkgs/core/Core/Stdio/fble-cat.out README.md"

  # /Core/Stdio/Test% interpreted test.
  test $::out/pkgs/core/Core/Stdio/fble-stdio.tr "$::out/pkgs/core/fble-stdio $::out/pkgs/core/Core/Stdio/Test.fble.d" \
    "$::out/pkgs/core/fble-stdio -I pkgs/core -m /Core/Stdio/Test% | grep PASSED"

  # /Core/Stdio/Test% compiled test.
  stdio $::out/pkgs/core/Core/Stdio/fble-stdio-test "/Core/Stdio/Test%" ""
  test $::out/pkgs/core/Core/Stdio/fble-stdio-test.out \
    $::out/pkgs/core/Core/Stdio/fble-stdio-test \
    "$::out/pkgs/core/Core/Stdio/fble-stdio-test > $::out/pkgs/core/Core/Stdio/fble-stdio-test.out"
  test $::out/pkgs/core/Core/Stdio/fble-stdio-test.tr $::out/pkgs/core/Core/Stdio/fble-stdio-test.out \
    "grep PASSED $::out/pkgs/core/Core/Stdio/fble-stdio-test.out"

  # Core/Tests interpreted
  testsuite $::out/pkgs/core/Core/tests.tr "$::out/pkgs/core/fble-stdio $::out/pkgs/core/Core/Tests.fble.d" \
    "$::out/pkgs/core/fble-stdio -I pkgs/core -m /Core/Tests% --prefix Interpreted."

  # Core/Tests compiled
  stdio $::out/pkgs/core/Core/core-tests "/Core/Tests%" ""
  testsuite $::out/pkgs/core/Core/core-tests.tr $::out/pkgs/core/Core/core-tests \
    "$::out/pkgs/core/Core/core-tests --prefix Compiled."
}
