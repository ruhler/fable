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

# ::d is for non-source files that should be included in the distribution
set ::d $::config::srcdir

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

# Files that the build.ninja file depends on.
set ::build_ninja_deps [list]

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
}

# bin --
#   Build a binary.
#
# Generates a buildstamp that can be refered to using:
#
#   extern const char* BUILDSTAMP;
#
# Inputs:
#   bin - the binary file to build.
#   objs - the list of .o and .a files to build from.
#   lflags - library flags, e.g. "-L foo/ -lfoo".
proc bin { bin objs lflags } {
  set cflags "-std=c99 -pedantic -Wall -Wextra -Wshadow -Werror -gdwarf-3 -ggdb -no-pie -O3"
  build $bin "$::s/buildstamp $objs" "$::s/buildstamp | gcc $cflags -o $bin $objs -x c - $lflags"
}

# bin_cov --
#   Build a binary with test coverage enabled.
#
# Generates a buildstamp that can be refered to using:
#
#   extern const char* BUILDSTAMP;
#
# Inputs:
#   bin - the binary file to build.
#   objs - the list of .o files to build from.
#   lflags - library flags, e.g. "-L foo/ -lfoo".
proc bin_cov { bin objs lflags } {
  #set cflags "-std=c99 -pedantic -Wall -Wextra -Wshadow -Werror -gdwarf-3 -ggdb -no-pie -fprofile-arcs -ftest-coverage -pg"
  set cflags "-std=c99 -pedantic -Wall -Wextra -Wshadow -Werror -gdwarf-3 -ggdb -fprofile-arcs -ftest-coverage"
  build $bin "$::s/buildstamp $objs" "$::s/buildstamp | gcc $cflags -o $bin $objs -x c - $lflags"
}

# install --
#   Installs a file to the given location.
#   Marks the destination as an 'install' target and the source as an 'all'
#   target.
set ::all [list]
set ::install [list]
proc install { src dest } {
  lappend ::all $src
  lappend ::install $dest
  build $dest $src "cp $src $dest"
}

# Mark a target as belonging to the www target.
set ::www [list]
proc www { target } {
  lappend ::www $target
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

# testsuite --
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

# Read the version string from fble-version.h
lappend ::build_ninja_deps $::s/include/fble/fble-version.h
set version_def [exec grep "#define FBLE_VERSION " $::s/include/fble/fble-version.h]
set ::version [lindex $version_def 2]

set ::dist [list]

# Mark a source file for distribution.
# For example: dist_s README.fbld
proc dist_s { file } {
  lappend ::dist $::b/$::version/$file
  build $::b/$::version/$file $::s/$file \
    "cp $::s/$file $::b/$::version/$file"
}

# Mark a non-source file for distribution.
# For example: dist_d README.txt
proc dist_d { file } {
  lappend ::dist $::b/$::version/$file
  build $::b/$::version/$file $::d/$file \
    "cp $::d/$file $::b/$::version/$file"
}

# Perform glob on files in the given dir.
# args are additional arguments passed to the standard tcl glob command.
proc build_glob {dir args} {
  lappend ::build_ninja_deps $dir 
  glob -directory $dir {*}$args
}

# Returns the list of all subdirectories, recursively, of the given directory.
# The 'root' directory will not be included as a prefix in the returned list
# of directories.
# 'dir' should be empty or end with '/'.
proc dirs { root dir } {
  set l [list $dir]
  foreach {x} [build_glob $root -tails -nocomplain -type d $dir*] {
    set l [concat $l [dirs $root "$x/"]]
  }
  return $l
}

# Source a build.tcl file.
proc build_tcl { file } {
  lappend ::build_ninja_deps $file
  source $file
}

build_tcl $::s/include/build.tcl
build_tcl $::s/lib/build.tcl
build_tcl $::s/bin/build.tcl
build_tcl $::s/fbld/build.tcl
build_tcl $::s/spec/build.tcl
build_tcl $::s/test/build.tcl
build_tcl $::s/test/spec-test.build.tcl
build_tcl $::s/tutorials/build.tcl
build_tcl $::s/pkgs/build.tcl

build_tcl $::s/book/build.tcl
build_tcl $::s/thoughts/build.tcl
build_tcl $::s/vim/build.tcl

dist_s build.tcl
dist_s configure
dist_s deps.tcl
dist_s README.fbld
dist_s TODO.txt

# README file www
::html_doc $::b/www/index.html $::s/README.fbld
www $::b/www/index.html

# Test summary.
build $::b/detail.tr $::tests "cat $::tests > $::b/detail.tr"
build $::b/summary.tr \
  "$::s/test/log $::b/detail.tr $::s/test/tests.tcl" \
  "$::s/test/log $::b/summary.tr tclsh8.6 $::s/test/tests.tcl < $::b/detail.tr"

# Release tarball
build $::b/$::version.tar.gz $::dist \
  "tar --create --gzip --directory $::b --file $::version.tar.gz $::version"

# Phony targets.
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

phony "all" $::all
phony "test" $::b/summary.tr
phony "www" $::www
phony "dist" $::b/$::version.tar.gz
phony "check" [list test all www]
phony "install" $::install
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
