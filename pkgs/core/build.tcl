namespace eval "pkgs/core" {
  # .c library files.
  set objs [list]
  foreach {x} { Core/char.fble Core/int.fble Core/stdio.fble Core/string.fble } {
    lappend objs $::out/pkgs/core/$x.o
    obj $::out/pkgs/core/$x.o pkgs/core/$x.c "-I fble/include -I pkgs/core"
  }
  pkg core [list] $objs

  # fble-stdio program.
  obj $::out/pkgs/core/Core/fble-stdio.o pkgs/core/Core/fble-stdio.c \
    "-I fble/include -I pkgs/core"
  bin $::out/pkgs/core/Core/fble-stdio \
    "$::out/pkgs/core/Core/fble-stdio.o $::out/pkgs/core/libfble-core.a $::out/fble/lib/libfble.a" ""

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

    build $target.s $::out/fble/bin/fble-compile \
      "$::out/fble/bin/fble-compile --main FbleStdioMain -m $path > $target.s"
    asm $target.o $target.s
    bin $target $objs ""
  }

  # /Core/Stdio/Cat% interpreted test.
  test $::out/pkgs/core/Core/Stdio/fble-cat.tr "$::out/pkgs/core/Core/fble-stdio $::out/pkgs/core/Core/Stdio/Cat.fble.d" \
    "$::out/pkgs/core/Core/fble-stdio -I pkgs/core -m /Core/Stdio/Cat% < README.txt > $::out/pkgs/core/Core/Stdio/fble-cat.out && cmp $::out/pkgs/core/Core/Stdio/fble-cat.out README.txt"

  # /Core/Stdio/Test% interpreted test.
  test $::out/pkgs/core/Core/Stdio/fble-stdio.tr "$::out/pkgs/core/Core/fble-stdio $::out/pkgs/core/Core/Stdio/Test.fble.d" \
    "$::out/pkgs/core/Core/fble-stdio -I pkgs/core -m /Core/Stdio/Test% > $::out/pkgs/core/Core/Stdio/fble-stdio.out && grep PASSED $::out/pkgs/core/Core/Stdio/fble-stdio.out"

  # /Core/Stdio/Test% compiled test.
  stdio $::out/pkgs/core/Core/Stdio/fble-stdio-test "/Core/Stdio/Test%" ""
  test $::out/pkgs/core/Core/Stdio/fble-stdio-test.tr $::out/pkgs/core/Core/Stdio/fble-stdio-test \
    "$::out/pkgs/core/Core/Stdio/fble-stdio-test > $::out/pkgs/core/Core/Stdio/fble-stdio-test.out && grep PASSED $::out/pkgs/core/Core/Stdio/fble-stdio-test.out"

  # Core/Tests interpreted
  testsuite $::out/pkgs/core/Core/tests.tr "$::out/pkgs/core/Core/fble-stdio $::out/pkgs/core/Core/Tests.fble.d" \
    "$::out/pkgs/core/Core/fble-stdio -I pkgs/core -m /Core/Tests% --prefix Interpreted."

  # Core/Tests compiled
  stdio $::out/pkgs/core/Core/core-tests "/Core/Tests%" ""
  testsuite $::out/pkgs/core/Core/core-tests.tr $::out/pkgs/core/Core/core-tests \
    "$::out/pkgs/core/Core/core-tests --prefix Compiled."
}
