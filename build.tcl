# ninja-based description of how to build fble and friends.
#
# First time setup:
#   tclsh8.6 build.tcl build.ninja
#
# Afterwards:
#   ninja

# Source configuration options.
# See 'configure' for the available configuration options.
# Everything defined here is in the ::config namespace.
source config.tcl

set ::s $::config::srcdir
set ::b $::config::builddir

set ::build_ninja_filename [lindex $argv 0]
set ::build_ninja [open "$::build_ninja_filename.tmp" "w"]
set ::arch "[exec arch]"

# build.ninja header.
puts $::build_ninja {
rule build
  description = $out
  command = $cmd

cflags = -std=c99 -pedantic -Wall -Wextra -Wshadow -Werror -gdwarf-3 -ggdb -O3
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

# phony --
#   Sets an phony target for building the given collection of dependencies.
#
# As per ninja, this should be called after the dependencies have been
# defined.
#
# Inputs:
#   target - the name of the phony target
#   dependencies - the dependencies to build as part of the phony target.
proc phony { target dependencies } {
  puts $::build_ninja "build $target: phony [join $dependencies]"
}

# Indicate that the target should be included in the list of default targets
# when running 'ninja all'
set ::all [list]
proc all { target } {
  lappend ::all $target
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
  set cflags "-std=c99 -pedantic -Wall -Werror -Wshadow -gdwarf-3 -ggdb -fprofile-arcs -ftest-coverage"
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
  all $lib
}

# bin --
#   Build a binary.
#
# Inputs:
#   bin - the binary file to build.
#   objs - the list of .o and .a files to build from.
#   lflags - library flags, e.g. "-L foo/ -lfoo".
proc bin { bin objs lflags } {
  set cflags "-std=c99 -pedantic -Wall -Wextra -Wshadow -Werror -gdwarf-3 -ggdb -no-pie -O3"
  build $bin $objs "gcc $cflags -o $bin $objs $lflags"
  all $bin
}

# bin_cov --
#   Build a binary with test coverage enabled.
#
# Inputs:
#   bin - the binary file to build.
#   objs - the list of .o files to build from.
#   lflags - library flags, e.g. "-L foo/ -lfoo".
proc bin_cov { bin objs lflags } {
  #set cflags "-std=c99 -pedantic -Wall -Wextra -Wshadow -Werror -gdwarf-3 -ggdb -no-pie -fprofile-arcs -ftest-coverage -pg"
  set cflags "-std=c99 -pedantic -Wall -Wextra -Wshadow -Werror -gdwarf-3 -ggdb -fprofile-arcs -ftest-coverage"
  build $bin $objs "gcc $cflags -o $bin $objs $lflags"
  all $bin
}

# install --
#   Installs a file to the given location.
#   Marks the destination as an 'install' target and the source as an 'all'
#   target.
set ::installs [list]
proc install { src dest } {
  all $src
  lappend ::installs $dest
  build $dest $src "cp $src $dest"
}

# install_man1 --
#   Cause the given man page to be installed when the install target is
#   invoked.
# Inputs:
#   target - the man page to install.
proc install_man1 { target } {
  set install_target $::config::mandir/man1/[file tail $target]
  install $target $install_target
}

# test --
#   Build a test result.
#
# Inputs:
#   tr - the .tr file to output the results to.
#   deps - depencies for the test.
#   cmd - the test command to run, which should exit 0 to indicate the test
#         passed and non-zero to indicate the test failed.
#
# Adds the .tr file to global list of tests.
set ::tests [list]
proc test { tr deps cmd } {
  lappend ::tests $tr
  set name [file rootname $tr]
  build $tr "$::s/test/log $::s/test/test $deps" \
    "$::s/test/log $tr $::s/test/test $name $cmd"
}

# test --
#   Build a test result from a test suite.
#
# Inputs:
#   tr - the .tr file to output the results to.
#   deps - depencies for the test.
#   cmd - the testsuite command to run, which should output @[...] test info,
#         exit 0 to indicate the test passed and non-zero to indicate the test
#         failed.
#
# Adds the .tr file to global list of tests.
proc testsuite { tr deps cmd } {
  lappend ::tests $tr
  build $tr "$::s/test/log $deps" \
    "$::s/test/log $tr $cmd"
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

# Compile a .fble file to .o via aarch64.
#
# Inputs:
#   obj - the name of the .o file to generate.
#   compile - the name of the fble-compile executable.
#   compileargs - args to pass to the fble compiler.
#   args - additional dependencies, typically including the .fble file.
proc fbleobj_aarch64 { obj compile compileargs args } {
  set s [string map {.o .s} $obj]
  build $s "$compile $args" "$compile $compileargs > $s"
  set cmd "as -o $obj $s"
  build $obj "$s $args" $cmd
}

# Compile a .fble file to .o via c.
#
# Inputs:
#   obj - the name of the .o file to generate.
#   compile - the name of the fble-compile executable.
#   compileargs - args to pass to the fble compiler.
#   args - additional dependencies, typically including the .fble file.
proc fbleobj_c { obj compile compileargs args } {
  set c [string map {.o .c} $obj]
  build $c "$compile $args" "$compile -t c $compileargs > $c"
  set cmd "gcc -c -o $obj -I $::s/include $c"
  build $obj "$c $args" $cmd
}

# Compile a .fble file to .o.
#
# Via whatever target it thinks is best.
#
# Inputs:
#   obj - the name of the .o file to generate.
#   compile - the name of the fble-compile executable.
#   compileargs - args to pass to the fble compiler.
#   args - additional dependencies, typically including the .fble file.
proc fbleobj { obj compile compileargs args } {
  if {$::arch == "aarch64"} {
    fbleobj_aarch64 $obj $compile $compileargs {*}$args
  } else {
    fbleobj_c $obj $compile $compileargs {*}$args
  }
}

# pkg --
#   Build an fble library package.
#
# Inputs:
#   name - the name of the package, such as 'app'.
#   deps - list of fble packages (without fble- prefix) this package depends
#          on.
#   objs - additional object files to include in the generated library.
proc pkg {name deps objs} {
  set cflags "-I $::s/pkgs/$name"
  foreach dep $deps {
    append cflags " -I $::s/pkgs/$dep"
  }

  foreach dir [dirs $::s/pkgs/$name ""] {
    lappend ::build_ninja_deps "$::s/pkgs/$name/$dir"
    foreach {x} [glob -tails -directory $::s/pkgs/$name -nocomplain -type f $dir/*.fble] {
      set mpath "/[file rootname $x]%"

      # Generate a .d file to capture dependencies.
      build $::b/pkgs/$name/$x.d "$::b/bin/fble-deps $::s/pkgs/$name/$x" \
        "$::b/bin/fble-deps $cflags -t $::b/pkgs/$name/$x.d -m $mpath > $::b/pkgs/$name/$x.d" \
        "depfile = $::b/pkgs/$name/$x.d"

      fbleobj $::b/pkgs/$name/$x.o $::b/bin/fble-compile "-c $cflags -m $mpath" $::b/pkgs/$name/$x.d
      lappend objs $::b/pkgs/$name/$x.o

      set target $::config::datadir/fble/$name/$x
      install $::s/pkgs/$name/$x $target
    }
  }

  lib $::b/pkgs/$name/libfble-$name.a $objs
}

# Any time we run glob over a directory, add that directory to this list.
# We need to make sure to include these directories as a dependency on the
# generation of build.ninja.
set ::build_ninja_deps [list]

set build_tcls [list \
  $::s/include/build.tcl \
  $::s/lib/build.tcl \
  $::s/bin/build.tcl \
  $::s/fbld/build.tcl \
  $::s/test/build.tcl \
  $::s/test/spec-test.build.tcl \
  $::s/tutorials/build.tcl \
  $::s/www/build.tcl \
  $::s/pkgs/core/build.tcl \
  $::s/pkgs/app/build.tcl \
  $::s/pkgs/md5/build.tcl \
  $::s/pkgs/sat/build.tcl \
  $::s/pkgs/hwdg/build.tcl \
  $::s/pkgs/games/build.tcl \
  $::s/pkgs/invaders/build.tcl \
  $::s/pkgs/pinball/build.tcl \
  $::s/pkgs/graphics/build.tcl \
]

foreach build_tcl $build_tcls {
  lappend ::build_ninja_deps $build_tcl
  source $build_tcl
}

# Test summary.
build $::b/detail.tr $::tests "cat $::tests > $::b/detail.tr"
build $::b/summary.tr \
  "$::s/test/log $::b/detail.tr $::s/test/tests.tcl" \
  "$::s/test/log $::b/summary.tr tclsh8.6 $::s/test/tests.tcl < $::b/detail.tr"

# Phony targets.
phony "all" $::all
phony "check" [list $::b/summary.tr all www]
phony "install" $::installs
puts $::build_ninja "default all"

# build.ninja
build "$::b/build.ninja" \
  "$::s/build.tcl $::b/config.tcl" \
  "tclsh8.6 $::s/build.tcl $::b/build.ninja" \
  "depfile = $::b/build.ninja.d"

# build.ninja.d implicit dependency file.
set ::build_ninja_d [open "$::b/build.ninja.d" "w"]
puts $::build_ninja_d "$::b/build.ninja: $::build_ninja_deps"

# Now that we've completed successfully, copy over the generated ninja file.
# We wait until the end to do this to ensure we don't generate a partial
# malformed ninja file, which is harder to recover from.
close $::build_ninja
exec mv $::build_ninja_filename.tmp $::build_ninja_filename
