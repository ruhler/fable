namespace eval "pkgs/core" {
  set objs [list]
  set cflags [exec pkg-config --cflags fble]

  # .c library files.
  foreach {x} { Core/char.fble Core/int.fble Core/stdio.fble Core/string.fble } {
    lappend objs $::out/pkgs/core/$x.o
    obj $::out/pkgs/core/$x.o pkgs/core/$x.c "$cflags -I pkgs/core"
  }

  pkg core $objs

  # fble-stdio program.
  obj $::out/pkgs/core/Core/fble-stdio.o pkgs/core/Core/fble-stdio.c \
    [exec pkg-config --cflags fble fble-core]
  bin $::out/pkgs/core/Core/fble-stdio "$::out/pkgs/core/Core/fble-stdio.o" \
    [exec pkg-config --static --libs fble fble-core] \
    "$::out/fble/lib/libfble.a $::out/pkgs/core/libfble-core.a"

  # Build an fble-stdio compiled binary.
  #
  # Inputs:
  #   target - the file to build.
  #   path - the module path to use as Stdio@ main.
  #   libs - additional pkg-config named libraries that this depends on.
  #   args - optional additional dependencies.
  proc ::stdio { target path libs args} {
    build $target.s $::out/fble/bin/fble-compile \
      "$::out/fble/bin/fble-compile --main FbleStdioMain -m $path > $target.s"
    asm $target.o $target.s
    bin $target "$target.o" \
      [exec pkg-config --static --libs fble fble-core {*}$libs] \
      "$::out/fble/lib/libfble.a $::out/pkgs/core/libfble-core.a" {*}$args
  }

  # /Core/Stdio/Cat% interpreted test.
  test $::out/pkgs/core/Core/Stdio/fble-cat.tr "$::out/pkgs/core/Core/fble-stdio $::out/pkgs/core/Core/Stdio/Cat.fble.d" \
    "$::out/pkgs/core/Core/fble-stdio -I pkgs/core -m /Core/Stdio/Cat% < README.txt > $::out/pkgs/core/Core/Stdio/fble-cat.out && cmp $::out/pkgs/core/Core/Stdio/fble-cat.out README.txt"

  # /Core/Stdio/Test% interpreted test.
  test $::out/pkgs/core/Core/Stdio/fble-stdio.tr "$::out/pkgs/core/Core/fble-stdio $::out/pkgs/core/Core/Stdio/Test.fble.d" \
    "$::out/pkgs/core/Core/fble-stdio -I pkgs/core -m /Core/Stdio/Test% > $::out/pkgs/core/Core/Stdio/fble-stdio.out && grep PASSED $::out/pkgs/core/Core/Stdio/fble-stdio.out > /dev/null"

  # /Core/Stdio/Test% compiled test.
  stdio $::out/pkgs/core/Core/Stdio/fble-stdio-test "/Core/Stdio/Test%" ""
  test $::out/pkgs/core/Core/Stdio/fble-stdio-test.tr $::out/pkgs/core/Core/Stdio/fble-stdio-test \
    "$::out/pkgs/core/Core/Stdio/fble-stdio-test > $::out/pkgs/core/Core/Stdio/fble-stdio-test.out && grep PASSED $::out/pkgs/core/Core/Stdio/fble-stdio-test.out > /dev/null"

  # Core/Tests interpreted
  test $::out/pkgs/core/Core/tests.tr "$::out/pkgs/core/Core/fble-stdio $::out/pkgs/core/Core/Tests.fble.d" \
    "$::out/pkgs/core/Core/fble-stdio -I pkgs/core -m /Core/Tests%" "pool = console"

  # Core/Tests compiled
  stdio $::out/pkgs/core/Core/core-tests "/Core/Tests%" "fble-core" \
    $::out/pkgs/core/libfble-core.a
  test $::out/pkgs/core/Core/core-tests.tr $::out/pkgs/core/Core/core-tests \
    "$::out/pkgs/core/Core/core-tests" "pool = console"
}
