namespace eval "pkgs/md5" {
  set objs [list]

  # .c library files.
  foreach {x} { Md5/md5.fble } {
    lappend objs $::out/pkgs/md5/$x.o
    obj $::out/pkgs/md5/$x.o pkgs/md5/$x.c "-I fble/include -I pkgs/core -I pkgs/md5"
  }

  pkg md5 [list core] $objs

  # Md5/Tests interpreted
  test $::out/pkgs/md5/Md5/tests.tr "$::out/pkgs/core/Core/fble-stdio $::out/pkgs/md5/Md5/Tests.fble.d" \
    "$::out/pkgs/core/Core/fble-stdio [exec pkg-config --cflags-only-I fble-md5] -m /Md5/Tests%" "pool = console"

  # Md5/Tests compiled
  stdio $::out/pkgs/md5/Md5/md5-tests "/Md5/Tests%" "fble-md5" \
    "$::out/pkgs/core/libfble-core.a $::out/pkgs/md5/libfble-md5.a"
  test $::out/pkgs/md5/Md5/md5-tests.tr $::out/pkgs/md5/Md5/md5-tests \
    "$::out/pkgs/md5/Md5/md5-tests" "pool = console"

  # fble-md5 program.
  obj $::out/pkgs/md5/Md5/fble-md5.o pkgs/md5/Md5/fble-md5.c \
    [exec pkg-config --cflags fble fble-core fble-md5]
  bin $::out/pkgs/md5/Md5/fble-md5 "$::out/pkgs/md5/Md5/fble-md5.o" \
    [exec pkg-config --static --libs fble-md5] \
    "$::out/fble/lib/libfble.a $::out/pkgs/core/libfble-core.a $::out/pkgs/md5/libfble-md5.a"

  # fble-md5 test
  test $::out/pkgs/md5/Md5/fble-md5.tr "$::out/pkgs/md5/Md5/fble-md5 $::out/pkgs/md5/Md5/Main.fble.d" \
    "$::out/pkgs/md5/Md5/fble-md5 -I pkgs/core -I pkgs/md5 -m /Md5/Main% /dev/null > $::out/pkgs/md5/Md5/fble-md5.out && grep d41d8cd98f00b204e9800998ecf8427e $::out/pkgs/md5/Md5/fble-md5.out > /dev/null"
}

