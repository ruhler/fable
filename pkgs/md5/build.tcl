namespace eval "pkgs/md5" {

  # .c library files.
  set objs [list]
  foreach {x} { md5.fble } {
    lappend objs $::b/pkgs/md5/$x.o
    obj $::b/pkgs/md5/$x.o $::s/pkgs/md5/$x.c "-I $::s/include/fble -I $::s/pkgs/core -I $::s/pkgs/md5"
  }
  pkg md5 [list core] $objs

  # Md5/Tests interpreted
  testsuite $::b/pkgs/md5/Md5/tests.tr "$::b/pkgs/core/fble-stdio $::b/pkgs/md5/Md5/Tests.fble.d" \
    "$::b/pkgs/core/fble-stdio -I $::s/pkgs/core -I $::s/pkgs/md5 -m /Md5/Tests% --prefix Interpreted."

  # Md5/Tests compiled
  stdio $::b/pkgs/md5/Md5/md5-tests "/Md5/Tests%" "md5"
  testsuite $::b/pkgs/md5/Md5/md5-tests.tr $::b/pkgs/md5/Md5/md5-tests \
    "$::b/pkgs/md5/Md5/md5-tests --prefix Compiled."

  # Md5/Bench compiled
  stdio $::b/pkgs/md5/Md5/md5-bench "/Md5/Bench%" "md5"

  # fble-md5 program.
  obj $::b/pkgs/md5/fble-md5.o $::s/pkgs/md5/fble-md5.c \
    "-I $::s/include/fble -I $::s/pkgs/core -I $::s/pkgs/md5"
  set objs "$::b/pkgs/md5/fble-md5.o"
  append objs " $::b/pkgs/md5/libfble-md5.a"
  append objs " $::b/pkgs/core/libfble-core.a"
  append objs " $::b/lib/libfble.a"
  bin $::b/pkgs/md5/fble-md5 $objs ""
  install_bin $::b/pkgs/md5/fble-md5

  # fble-md5 test
  build $::b/pkgs/md5/fble-md5.out \
    "$::b/pkgs/md5/fble-md5 $::b/pkgs/md5/Md5/Main.fble.d" \
    "$::b/pkgs/md5/fble-md5 -I $::s/pkgs/core -I $::s/pkgs/md5 -m /Md5/Main% /dev/null > $::b/pkgs/md5/fble-md5.out"
  test $::b/pkgs/md5/fble-md5.tr \
    "$::b/pkgs/md5/fble-md5.out" \
    "grep d41d8cd98f00b204e9800998ecf8427e $::b/pkgs/md5/fble-md5.out"
}

