# ninja-based description of how to build fble and friends.
#
# First time setup:
#   tclsh build.ninja
#
# Afterwards:
#   ninja -f out/build.ninja

# output directory used for build.
set ::out "out"

exec mkdir -p $::out
set ::build_ninja [open "$::out/build.ninja" "w"]

# build.ninja header.
puts $::build_ninja "builddir = $::out"
puts $::build_ninja {
rule build
  description = $out
  command = $cmd

cflags = -std=c99 -pedantic -Wall -Werror -gdwarf-3 -ggdb -O3
rule obj
  description = $out
  command = gcc -MMD -MF $out.d $cflags $iflags -c -o $out $src
  depfile = $out.d
}

# build --
#   Builds generic targets.
#
# Inputs:
#   targets - the list of targets produced by the command.
#   dependencies - the list of targets this depends on.
#   command - the command to run to produce the targets.
#   args - optional additional "key = argument" pairs to use for ninja rule.
proc build { targets dependencies command args } {
  puts $::build_ninja "build [join $targets]: build [join $dependencies]"
  puts $::build_ninja "  cmd = $command"

  foreach kv $args {
    puts $::build_ninja "  $kv"
  }
}

# obj --
#   Builds a .o file.
#
# Inputs:
#   obj - the .o file to build.
#   src - the .c file to build the .o file from.
#   iflags - include flags, e.g. "-I foo".
#   args - optional additional dependencies.
proc obj { obj src iflags args } {
  puts $::build_ninja "build $obj: obj $src $args"
  puts $::build_ninja "  src=$src"
  puts $::build_ninja "  iflags=$iflags"
}

# obj_cov --
#   Builds a .o file with test coverage enabled.
#
# Inputs:
#   obj - the .o file to build.
#   src - the .c file to build the .o file from.
#   iflags - include flags, e.g. "-I foo".
#   args - optional additional dependencies.
proc obj_cov { obj src iflags args } {
  set gcda [string map {.o .gcda} $obj]
  set cflags "-std=c99 -pedantic -Wall -Werror -gdwarf-3 -ggdb -fprofile-arcs -ftest-coverage"
  set cmd "rm -f $gcda ; gcc -MMD -MF $obj.d $cflags $iflags -c -o $obj $src"
  build $obj "$src $args" $cmd "depfile = $obj.d"
}

# Compile a .s file to .o
#
# Inputs:
#   obj - the .o file to build.
#   src - the .s file to build the .o file from.
#   args - optional additional dependencies.
proc asm { obj src args } {
  set cmd "as -o $obj $src"
  build $obj "$src $args" $cmd
}

# lib --
#   Build a library.
#
# Inputs:
#   lib - the library file to build
#   objs - the list of .o files to include in the library.
proc lib { lib objs } {
  build $lib $objs "rm -f $lib ; ar rcs $lib $objs"
}

# bin --
#   Build a binary.
#
# Inputs:
#   bin - the binary file to build.
#   objs - the list of .o and .a files to build from.
#   lflags - library flags, e.g. "-L foo/ -lfoo".
#   args - optional additional dependencies. TODO: remove this.
proc bin { bin objs lflags args } {
  #set cflags "-std=c99 -pedantic -Wall -Werror -gdwarf-3 -ggdb -no-pie -fprofile-arcs -ftest-coverage -pg"
  set cflags "-std=c99 -pedantic -Wall -Werror -gdwarf-3 -ggdb -no-pie -O3"
  build $bin "$objs $args" "gcc $cflags -o $bin $objs $lflags"
}

# bin_cov --
#   Build a binary with test coverage enabled.
#
# Inputs:
#   bin - the binary file to build.
#   objs - the list of .o files to build from.
#   lflags - library flags, e.g. "-L foo/ -lfoo".
#   args - optional additional dependencies.
proc bin_cov { bin objs lflags args } {
  set cflags "-std=c99 -pedantic -Wall -Werror -gdwarf-3 -ggdb -fprofile-arcs -ftest-coverage"
  build $bin "$objs $args" "gcc $cflags -o $bin $objs $lflags"
}

# test --
#   Build a test result.
#
# Inputs:
#   tr - the .tr file to output the results to.
#   deps - depencies for the test.
#   cmd - the test command to run, which should exit 0 to indicate the test
#         passed and non-zero to indicate the test failed.
#   args - optional additional "key = argument" pairs to use for ninja rule.
#
# Adds the .tr file to global list of tests.
set ::tests [list]
proc test { tr deps cmd args} {
  lappend ::tests $tr
  build $tr $deps "$cmd && echo PASSED > $tr" {*}$args
}

# Returns the list of all subdirectories, recursively, of the given directory.
# The 'root' directory will not be included as a prefix in the returned list
# of directories.
# 'dir' should be empty or end with '/'.
proc dirs { root dir } {
  set l [list $dir]
  foreach {x} [glob -tails -directory $root -nocomplain -type d $dir*] {
    set l [concat $l [dirs $root "$x/"]]
  }
  return $l
}

# Compile a .fble file to .o.
#
# Inputs:
#   obj - the name of the .o file to generate.
#   compile - the name of the fble-compile executable.
#   compileargs - args to pass to the fble compiler.
#   args - additional dependencies, typically including the .fble file.
proc fbleobj { obj compile compileargs args } {
  set s [string map {.o .s} $obj]
  build $s "$compile $args" "$compile $compileargs > $s"

  set cmd "as -o $obj $s"
  build $obj "$s $args" $cmd
}

# pkg --
#   Build an fble library package.
#
# Inputs:
#   name - the name of the package, such as 'app'.
#   objs - additional object files to include in the generated library.
proc pkg {name objs} {
  set cflags [exec pkg-config --cflags-only-I fble-$name]
  foreach dir [dirs pkgs/$name ""] {
    lappend ::build_ninja_deps "pkgs/$name/$dir"
    foreach {x} [glob -tails -directory pkgs/$name -nocomplain -type f $dir/*.fble] {
      set mpath "/[file rootname $x]%"

      # Generate a .d file to capture dependencies.
      build $::out/pkgs/$name/$x.d "$::out/fble/bin/fble-deps pkgs/$name/$x" \
        "$::out/fble/bin/fble-deps $cflags -t $::out/pkgs/$name/$x.d -m $mpath > $::out/pkgs/$name/$x.d" \
        "depfile = $::out/pkgs/$name/$x.d"

      fbleobj $::out/pkgs/$name/$x.o $::out/fble/bin/fble-compile "-c $cflags -m $mpath" $::out/pkgs/$name/$x.d
      lappend objs $::out/pkgs/$name/$x.o
    }
  }

  lib $::out/pkgs/$name/libfble-$name.a $objs
}

# Any time we run glob over a directory, add that directory to this list.
# We need to make sure to include these directories as a dependency on the
# generation of build.ninja.
set ::build_ninja_deps [list]

# Set up pkg-config for use in build.
set ::env(PKG_CONFIG_TOP_BUILD_DIR) $::out
set ::env(PKG_CONFIG_PATH) fble
lappend ::build_ninja_deps "fble/fble.pc"
lappend ::build_ninja_deps "fble/fble.cov.pc"
foreach pkg [list core sat app hwdg invaders pinball games graphics md5] {
  append ::env(PKG_CONFIG_PATH) ":pkgs/$pkg"
  lappend ::build_ninja_deps "pkgs/$pkg/fble-$pkg.pc"
}

set build_dirs {
  fble/lib fble/bin fble/test
  langs
}
foreach build_dir $build_dirs {
  lappend ::build_ninja_deps "$build_dir/build.tcl"
  source $build_dir/build.tcl
}

# fble 'core' library package.
eval {
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
  proc stdio { target path libs args} {
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

# fble 'app' library package.
eval {
  set objs [list]

  # .c library files.
  foreach {x} { App/app.fble } {
    lappend objs $::out/pkgs/app/$x.o
    obj $::out/pkgs/app/$x.o pkgs/app/$x.c "-I /usr/include/SDL2 -I fble/include -I pkgs/core -I pkgs/app"
  }

  pkg app $objs

  # fble-app program.
  obj $::out/pkgs/app/App/fble-app.o pkgs/app/App/fble-app.c \
    [exec pkg-config --cflags fble fble-core fble-app]
  bin $::out/pkgs/app/App/fble-app "$::out/pkgs/app/App/fble-app.o" \
    [exec pkg-config --static --libs fble-app] \
    "$::out/fble/lib/libfble.a $::out/pkgs/core/libfble-core.a $::out/pkgs/app/libfble-app.a"

  # Build an fble-app compiled binary.
  #
  # Inputs:
  #   target - the file to build.
  #   path - the module path to use as App@ main.
  #   libs - addition pkg-config named libraries to depend on.
  #   args - optional additional dependencies.
  proc app { target path libs args} {
    build $target.s $::out/fble/bin/fble-compile \
      "$::out/fble/bin/fble-compile --main FbleAppMain -m $path > $target.s"
    asm $target.o $target.s
    bin $target "$target.o" \
      [exec pkg-config --static --libs fble-app {*}$libs] \
      "$::out/fble/lib/libfble.a $::out/pkgs/core/libfble-core.a $::out/pkgs/app/libfble-app.a $args"
  }
}

# fble 'md5' library package.
eval {
  set objs [list]

  # .c library files.
  foreach {x} { Md5/md5.fble } {
    lappend objs $::out/pkgs/md5/$x.o
    obj $::out/pkgs/md5/$x.o pkgs/md5/$x.c "-I fble/include -I pkgs/core -I pkgs/md5"
  }

  pkg md5 $objs

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

# sat package
eval {
  pkg sat ""

  # /Sat/Tests% interpreted
  test $::out/pkgs/sat/Sat/tests.tr "$::out/pkgs/core/Core/fble-stdio $::out/pkgs/sat/Sat/Tests.fble.d" \
    "$::out/pkgs/core/Core/fble-stdio [exec pkg-config --cflags-only-I fble-sat] -m /Sat/Tests%" "pool = console"

  # /Sat/Tests% compiled
  stdio $::out/pkgs/sat/Sat/sat-tests "/Sat/Tests%" "fble-sat" \
    "$::out/pkgs/core/libfble-core.a $::out/pkgs/sat/libfble-sat.a"
  test $::out/pkgs/sat/Sat/sat-tests.tr $::out/pkgs/sat/Sat/sat-tests \
    "$::out/pkgs/sat/Sat/sat-tests" "pool = console"
}

# hwdg package
eval {
  pkg hwdg ""

  # /Hwdg/Tests% interpreted
  test $::out/pkgs/hwdg/Hwdg/tests.tr "$::out/pkgs/core/Core/fble-stdio $::out/pkgs/hwdg/Hwdg/Tests.fble.d" \
    "$::out/pkgs/core/Core/fble-stdio [exec pkg-config --cflags-only-I fble-hwdg] -m /Hwdg/Tests%" "pool = console"

  # /Hwdg/Tests% compiled
  stdio $::out/pkgs/hwdg/Hwdg/hwdg-tests "/Hwdg/Tests%" "fble-hwdg" \
    "$::out/pkgs/core/libfble-core.a $::out/pkgs/hwdg/libfble-hwdg.a"
  test $::out/pkgs/hwdg/Hwdg/hwdg-tests.tr $::out/pkgs/hwdg/Hwdg/hwdg-tests \
    "$::out/pkgs/hwdg/Hwdg/hwdg-tests" "pool = console"
}

# games package
eval {
  pkg games ""

  # /Games/Tests% interpreted
  test $::out/pkgs/games/Games/tests.tr "$::out/pkgs/core/Core/fble-stdio $::out/pkgs/games/Games/Tests.fble.d" \
    "$::out/pkgs/core/Core/fble-stdio [exec pkg-config --cflags-only-I fble-games] -m /Games/Tests%" "pool = console"

  # /Games/Tests% compiled
  stdio $::out/pkgs/games/Games/games-tests "/Games/Tests%" "fble-games" \
    "$::out/pkgs/core/libfble-core.a $::out/pkgs/app/libfble-app.a $::out/pkgs/games/libfble-games.a"
  test $::out/pkgs/games/Games/games-tests.tr $::out/pkgs/games/Games/games-tests \
    "$::out/pkgs/games/Games/games-tests" "pool = console"
}

# invaders package
eval {
  pkg invaders ""
  app $::out/pkgs/invaders/Invaders/fble-invaders "/Invaders/App%" \
    "fble-invaders" $::out/pkgs/invaders/libfble-invaders.a

  # /Invaders/Tests% interpreted
  test $::out/pkgs/invaders/Invaders/tests.tr "$::out/pkgs/core/Core/fble-stdio $::out/pkgs/invaders/Invaders/Tests.fble.d" \
    "$::out/pkgs/core/Core/fble-stdio [exec pkg-config --cflags-only-I fble-invaders] -m /Invaders/Tests%" "pool = console"

  # /Invaders/Tests% compiled
  stdio $::out/pkgs/invaders/Invaders/invaders-tests "/Invaders/Tests%" "fble-invaders" \
    "$::out/pkgs/core/libfble-core.a $::out/pkgs/app/libfble-app.a $::out/pkgs/invaders/libfble-invaders.a"
  test $::out/pkgs/invaders/Invaders/invaders-tests.tr $::out/pkgs/invaders/Invaders/invaders-tests \
    "$::out/pkgs/invaders/Invaders/invaders-tests" "pool = console"
}

# pinball package
eval {
  pkg pinball ""
  app $::out/pkgs/pinball/Pinball/fble-pinball "/Pinball/App%" \
    "fble-pinball" $::out/pkgs/pinball/libfble-pinball.a

  # /Pinball/Tests% interpreted
  test $::out/pkgs/pinball/Pinball/tests.tr "$::out/pkgs/core/Core/fble-stdio $::out/pkgs/pinball/Pinball/Tests.fble.d" \
    "$::out/pkgs/core/Core/fble-stdio [exec pkg-config --cflags-only-I fble-pinball] -m /Pinball/Tests%" "pool = console"

  # /Pinball/Tests% compiled
  stdio $::out/pkgs/pinball/Pinball/pinball-tests "/Pinball/Tests%" "fble-pinball" \
    "$::out/pkgs/core/libfble-core.a $::out/pkgs/app/libfble-app.a $::out/pkgs/pinball/libfble-pinball.a"
  test $::out/pkgs/pinball/Pinball/pinball-tests.tr $::out/pkgs/pinball/Pinball/pinball-tests \
    "$::out/pkgs/pinball/Pinball/pinball-tests" "pool = console"
}

# graphics package
eval {
  pkg graphics ""
  app $::out/pkgs/graphics/Graphics/fble-graphics "/Graphics/App%" \
    "fble-graphics" $::out/pkgs/graphics/libfble-graphics.a
}


# test summary
build $::out/tests.txt "$::tests" "echo $::tests > $::out/tests.txt"
build $::out/summary.tr "fble/test/tests.tcl $::out/tests.txt" \
  "tclsh fble/test/tests.tcl $::out/tests.txt && echo PASSED > $::out/summary.tr"

# build.ninja
build $::out/build.ninja "build.tcl" "tclsh build.tcl" \
  "depfile = $::out/build.ninja.d"

# build.ninja.d implicit dependency file.
set ::build_ninja_d [open "$::out/build.ninja.d" "w"]
puts $::build_ninja_d "$::out/build.ninja: $::build_ninja_deps"

