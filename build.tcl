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
  set cflags "-fPIC -std=c99 -pedantic -Wall -Werror -Wshadow -gdwarf-3 -ggdb -O3"
  set cmd "gcc -MMD -MF $obj.d $cflags $iflags -c -o $obj $src"
  build $obj "$src $args" $cmd "depfile = $obj.d"
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
  set cflags "-fPIC -std=c99 -pedantic -Wall -Werror -Wshadow -gdwarf-3 -ggdb --coverage"
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

# The filename extension to use for libraries on this platform.
set ::lext ".a"
#set ::lext ".so"

# lib --
#   Build a library.
#
# Inputs:
#   lib - the library file to build
#   objs - the list of .o files to include in the library.
proc lib { lib objs } {
  build $lib $objs "rm -f $lib ; ar rcs $lib $objs"
  #build $lib $objs "gcc -o $lib -shared $objs"
}

# lib_cov --
#   Builds a library for code coverage.
#
# Inputs:
#   lib - the library file to build
#   objs - the list of .o files to include in the library.
proc lib_cov { lib objs } {
  build $lib $objs "rm -f $lib ; ar rcs $lib $objs"
  #build $lib $objs "gcc -o $lib -shared $objs --coverage"
}

# Helper function to get extra flags for linking against local .so files.
# Inputs:
#   bin - the binary to produce
#   libs - the list of locally built .so files to link against.
proc libsflags { bin libs } {
  # We add rpaths to the binary so it can find the shared libs when run from
  # the build directory and from the install directory.
  set rpaths [list]
  if {[llength $libs] > 0} {
    # We assume all binaries are installed to $prefix/bin and libraries
    # installed to $prefix/lib, so that libraries can be found at ../lib at
    # install time.
    lappend rpaths "../lib"
  }

  # The directory of the binary, relative to $::b.
  if {[regexp $::b/(.*)/\[^/\]*\$ $bin m_all bindir] != 1} {
    error "invalid bin specified: $bin"
  }

  set lflags ""
  foreach lib $libs {
    # The library should match the pattern $::b/<path>/lib<name>.so
    set m [regexp "$::b/(.*)/lib(.*)$::lext" $lib m_all m_path m_lib]
    if {$m != 1} {
      error "invalid lib specified: $lib"
    }

    append lflags " -L $::b/$m_path -l$m_lib"

    set reldirs [list]
    foreach d [file split $bindir] {
      lappend reldirs ".."
    }
    set reldir "[join $reldirs /]/$m_path"
    if {[lsearch $rpaths $reldir] == -1} {
      lappend rpaths $reldir
    }
  }

  set rpath ""
  foreach p $rpaths {
    append rpath " -Wl,-rpath,\\\$\$ORIGIN/$p"
  }

  return "$rpath $lflags"
}

# bin --
#   Build a binary.
#
# Inputs:
#   bin - the binary file to build.
#   objs - the list of .o and .a files to build from.
#   libs - list of local .so files to link against.
#   lflags - additional library flags, e.g. "-L foo/ -lfoo".
#   args - additional dependencies
proc bin { bin objs libs lflags args } {
  set cflags "-std=c99 $::config::ldflags -pedantic -Wall -Wextra -Wshadow -Werror -gdwarf-3 -ggdb -O3"
  build $bin "$objs $libs $args" "gcc $cflags -o $bin $objs [::libsflags $bin $libs] $lflags"
}

# bin_cov --
#   Build a binary with test coverage enabled.
#
# Inputs:
#   bin - the binary file to build.
#   objs - the list of .o files to build from.
#   lflags - library flags, e.g. "-L foo/ -lfoo".
#   args - additional dependencies
proc bin_cov { bin objs libs lflags args } {
  set cflags "-std=c99 $::config::ldflags --pedantic -Wall -Wextra -Wshadow -Werror -gdwarf-3 -ggdb --coverage"
  build $bin "$objs $libs $args" "gcc $cflags -o $bin $objs [::libsflags $bin $libs] $lflags"
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
# The target name should be specified relative to the www/ directory.
set ::www [list]
proc www { target } {
  lappend ::www $::b/www/$target
  install $::b/www/$target $::config::docdir/fble/$target
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

build_tcl $::s/fbld/build.tcl
build_tcl $::s/include/build.tcl
build_tcl $::s/lib/build.tcl
build_tcl $::s/bin/build.tcl
build_tcl $::s/test/build.tcl
build_tcl $::s/test/spec-test.build.tcl
build_tcl $::s/pkgs/build.tcl
build_tcl $::s/spec/build.tcl
build_tcl $::s/tutorials/build.tcl
build_tcl $::s/book/build.tcl

# README.md
build $::b/README.gen.md \
  "$::fbld $::s/fbld/nobuildstamp.fbld $::b/fbld/version.fbld $::s/fbld/markdown.fbld $::s/README.fbld" \
  "$::fbld $::s/fbld/nobuildstamp.fbld $::b/fbld/version.fbld $::s/fbld/markdown.fbld $::s/README.fbld > $::b/README.gen.md"

# Make sure the version of README.md checked in matches the latest version.
test $::b/README.tr \
  "$::s/README.md $::b/README.gen.md" \
  "diff --strip-trailing-cr $::s/README.md $::b/README.gen.md"

# README file www
fbld_html_doc $::b/www/README.html $::s/README.fbld
www README.html
build $::b/www/index.html $::b/www/README.html \
  "cp $::b/www/README.html $::b/www/index.html"
www index.html

# Release file www
fbld_html_doc $::b/www/Release.html $::s/Release.fbld
www Release.html

# Test summary.
build $::b/detail.tr $::tests "cat $::tests > $::b/detail.tr"
build $::b/summary.tr \
  "$::s/test/log $::b/detail.tr $::s/test/tests.tcl" \
  "$::s/test/log $::b/summary.tr tclsh8.6 $::s/test/tests.tcl < $::b/detail.tr"

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
phony "check" [list all test www]
phony "install" $::install
puts $::build_ninja "default all"

# build.ninja
build "$::b/build.ninja" \
  "$::s/build.tcl $::b/config.tcl" \
  "tclsh8.6 $::s/build.tcl $::b/build.ninja" \
  "depfile = $::b/build.ninja.d" \
  "generator = 1"

# build.ninja.d implicit dependency file.
set ::build_ninja_d [open "$::b/build.ninja.d" "w"]
puts $::build_ninja_d "$::b/build.ninja: $::build_ninja_deps"

# Now that we've completed successfully, copy over the generated ninja file.
# We wait until the end to do this to ensure we don't generate a partial
# malformed ninja file, which is harder to recover from.
close $::build_ninja
exec mv $::build_ninja_filename.tmp $::build_ninja_filename

# Optimistically run the cleandead tool to reduce the chance of breaking
# incremental builds when removing targets. Ignore the output and exit status
# to avoid spamming users with older versions of ninja that don't have the
# cleandead tool.
catch "exec ninja -C $::b -t cleandead"
