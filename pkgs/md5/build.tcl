namespace eval "pkgs/md5" {

  # .c library files.
  set objs [list]
  foreach {x} { md5.fble } {
    lappend objs $::out/pkgs/md5/$x.o
    obj $::out/pkgs/md5/$x.o pkgs/md5/$x.c "-I fble/include -I pkgs/core -I pkgs/md5"
  }
  pkg md5 [list core] $objs

  # Md5/Tests interpreted
  testsuite $::out/pkgs/md5/Md5/tests.tr "$::out/pkgs/core/fble-stdio $::out/pkgs/md5/Md5/Tests.fble.d" \
    "$::out/pkgs/core/fble-stdio -I pkgs/core -I pkgs/md5 -m /Md5/Tests% --prefix Interpreted."

  # Md5/Tests compiled
  stdio $::out/pkgs/md5/Md5/md5-tests "/Md5/Tests%" "md5"
  testsuite $::out/pkgs/md5/Md5/md5-tests.tr $::out/pkgs/md5/Md5/md5-tests \
    "$::out/pkgs/md5/Md5/md5-tests --prefix Compiled."

  # Md5/Bench compiled
  stdio $::out/pkgs/md5/Md5/md5-bench "/Md5/Bench%" "md5"

  # fble-md5 program.
  obj $::out/pkgs/md5/fble-md5.o pkgs/md5/fble-md5.c \
    "-I fble/include -I pkgs/core -I pkgs/md5"
  set objs "$::out/pkgs/md5/fble-md5.o"
  append objs " $::out/pkgs/md5/libfble-md5.a"
  append objs " $::out/pkgs/core/libfble-core.a"
  append objs " $::out/fble/lib/libfble.a"
  bin $::out/pkgs/md5/fble-md5 $objs ""

  # fble-md5 test
  build $::out/pkgs/md5/fble-md5.out \
    "$::out/pkgs/md5/fble-md5 $::out/pkgs/md5/Md5/Main.fble.d" \
    "$::out/pkgs/md5/fble-md5 -I pkgs/core -I pkgs/md5 -m /Md5/Main% /dev/null > $::out/pkgs/md5/fble-md5.out"
  test $::out/pkgs/md5/fble-md5.tr \
    "$::out/pkgs/md5/fble-md5.out" \
    "grep d41d8cd98f00b204e9800998ecf8427e $::out/pkgs/md5/fble-md5.out"
}

