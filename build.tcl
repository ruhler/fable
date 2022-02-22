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
# TODO: Figure out how to remove use of pkg-config in build and do things
# directly in tcl instead.
set ::env(PKG_CONFIG_TOP_BUILD_DIR) $::out
set ::env(PKG_CONFIG_PATH) fble
lappend ::build_ninja_deps "fble/fble.pc"
foreach pkg [list core sat app hwdg invaders pinball games graphics md5] {
  append ::env(PKG_CONFIG_PATH) ":pkgs/$pkg"
  lappend ::build_ninja_deps "pkgs/$pkg/fble-$pkg.pc"
}

set build_dirs {
  fble/lib fble/bin fble/test
  langs
  pkgs/core pkgs/app pkgs/md5 pkgs/sat pkgs/hwdg pkgs/games
  pkgs/invaders pkgs/pinball pkgs/graphics
}

foreach build_dir $build_dirs {
  lappend ::build_ninja_deps "$build_dir/build.tcl"
  source $build_dir/build.tcl
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

