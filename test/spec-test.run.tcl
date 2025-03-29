# spec-test.run.tcl
#
# Helper script to run a spec test.
#
# Usage:
#   tclsh8.6 srcdir builddir test-spec.tcl FBLE
#
#   srcdir - Path to the source directory containing the spec/ directory.
#   builddir - Path to the build directory.
#   FBLE - Path to the fble test file, relative to spec/ directory.
#
# Example:
#
#   tclsh8.6 .. . spec-test.run.tcl SpecTests/00_Test/NoError.fble
#
# Parses the @@fble-test@@ metadata of $srcdir/spec/$FBLE and executes the
# test appropriately. Reports an error in case of test failure.
#
# This script assumes it is run from the build directory, with access to
# fble binaries in the build directory and $builddir/spec/${FBLE}.d the result
# of running fble-deps on the FBLE file.

set ::arch "[exec arch]"

source config.tcl

proc module_path { path } {
  # Add quotes around module path words so we can name spec tests to match
  # spec section numbers, e.g. 2.2-Kinds.
  return "/\'[string map { "/" "\'/\'"} [file rootname $path]]\'%"
}

proc execv { args } {
  puts $args
  exec {*}$args
}

# expect_error
#   Helper function for expected error case.
#
# Inputs:
#   type - type of error expected: compile or runtime
#   loc - location of expected error.
#   args - command to run.
proc expect_error { type loc args } {
  switch $type {
    compile { set es 2 }
    runtime { set es 3 }
    default { error "unsupported error type: $type" }
  }

  set output ""
  set status 0
  try {
    execv {*}$args 2>@1
  } trap CHILDSTATUS {results options} {
    set output $results
    set status [lindex [dict get $options -errorcode] 2]
  }

  if {$status == 0} {
    puts "@FAILED"
    error "Expected $type error, but no error encountered."
  }

  if {$status != $es} {
    puts "@FAILED"
    error "Expected $type error, but got:\n$output"
  }

  if {-1 == [string first ":$::loc: error" $output]} {
    puts "@FAILED"
    error "Expected error at $::loc, but got:\n$output"
  }
}

# expect_warning
#   Helper function for expected warning case.
#
# Inputs:
#   loc - location of expected warning.
#   args - command to run.
proc expect_warning { loc args } {
  set output [execv {*}$args 2>@1]

  if {-1 == [string first ":$::loc: warning" $output]} {
    puts "@FAILED"
    error "Expected warning at $::loc, but got:\n$output"
  }
}

set ::s [lindex $argv 0]
set ::b [lindex $argv 1]
set ::fble [lindex $argv 2]
set ::path $::s/spec/$fble
set ::mpath [module_path $fble]

puts "@\[$fble\]"

# Directory to use for any artifacts from the test run.
set ::outdir $::b/spec/[file rootname $fble]
exec mkdir -p $outdir

# Parse the @@fble-test@@ metadata.
set type "???"
set loc ""
set fin [open "$path" "r"]
set test_line 1
set passed "@PASSED"
while {[gets $fin line] >= 0} {
  if {[string first "@@fble-test@@ xfail" $line] != -1} {
    set passed "@XFAILED"
    continue
  }

  set first [string first "@@fble-test@@" $line]
  if {$first != -1} {
    set index [expr $first + [string length "@@fble-test@@"]]
    set metadata [string trim [string range $line $index end]]
    set type $metadata
    set loc ""
    set space [string first " " $metadata]
    if {$space != -1} {
      set type [string trim [string range $metadata 0 $space]]
      set loc [string trim [string range $metadata $space end]]
    }
    break
  }
  incr test_line
}
close $fin

# compile -- 
#   Compiles the test .fble file into an $outdir/compiled binary.
#
# Inputs:
#   target - "aarch64" or "c", the compilation target to use.
#   main - FbleTestMain or FbleMemTestMain.
#
# Results:
#   The path to the compiled executable.
proc compile {target main} {
  # Parse the list of .fble files the test depends on from the .d file.
  set fbles [list]
  foreach x [lrange [split [read [open $::b/spec/$::fble.d "r"]]] 1 end] {
    if {[string match *.fble $x]} {
      lappend fbles [string map [list "$::s/spec/" ""] $x]
    }
  }

  set objs [list]
  foreach fble $fbles {
    set name [string map { "/" "-" } $fble]
    set m [module_path $fble]
    set exe $::outdir/$name.$target
    set obj $exe.o
    switch $target {
      aarch64 { set out $exe.s }
      c       { set out $exe.c }
    }
    set flags [list]
    if {$fble == $::fble} {
      lappend flags "--main"
      lappend flags $main
      set main_exe $exe
    }
    execv $::b/bin/fble-compile.cov {*}$flags -t $target -c -I $::s/spec -m $m > $out

    switch $target {
      aarch64 { exec as -o $obj $out }
      c { exec gcc -gdwarf-3 -ggdb -c -o $obj -I $::s/include -I $::s/lib $out }
    }
    lappend objs $obj
  }

  exec gcc {*}$::config::ldflags --pedantic -Wall -Werror -gdwarf-3 -ggdb --coverage -O0 -o $main_exe {*}$objs -Wl,-rpath,$::b/test -L $::b/test -lfbletest -Wl,-rpath,$::b/lib -L $::b/lib -lfble.cov
  return $main_exe
}

# compile_and_run --
#   Compiles the test and runs it for each available target architecture.
#
# Inputs:
#   main - FbleTestMain or FbleMemTestMain.
#   body - The body of a tcl proc that takes the compiled executable as an
#          argument and runs the compiled code as desired.
proc compile_and_run {main body} {
  proc do_body {compiled} $body

  if {$::arch == "aarch64"} {
    set compiled [compile aarch64 $main]
    do_body $compiled
  }

  set compiled [compile c $main]
  do_body $compiled
}

proc dispatch {} {
  switch $::type {
    no-error {
      execv $::b/test/fble-test.cov --profile $::outdir/profile.txt -I $::s/spec -m $::mpath
      compile_and_run FbleTestMain { execv $compiled --profile $::outdir/profile.txt }
      execv $::b/bin/fble-disassemble.cov -I $::s/spec -m $::mpath
    }

    compile-error {
      expect_error compile $::loc $::b/test/fble-test.cov -I $::s/spec -m $::mpath
    }

    compile-warning {
      expect_warning $::loc $::b/test/fble-test.cov -I $::s/spec -m $::mpath
    }

    runtime-error {
      expect_error runtime $::loc $::b/test/fble-test.cov -I $::s/spec -m $::mpath
      compile_and_run FbleTestMain { expect_error runtime $::loc $compiled }
      execv $::b/bin/fble-disassemble.cov -I $::s/spec -m $::mpath
    }

    compile-module {
      execv $::b/bin/fble-disassemble.cov -I $::s/spec -m $::mpath
    }
    
    memory-constant {
      execv $::b/test/fble-mem-test.cov -I $::s/spec -m $::mpath
      compile_and_run FbleMemTestMain { execv $compiled }
      execv $::b/bin/fble-disassemble.cov -I $::s/spec -m $::mpath
    }

    memory-growth {
      execv $::b/test/fble-mem-test.cov --growth -I $::s/spec -m $::mpath
      compile_and_run FbleMemTestMain { execv $compiled --growth }
      execv $::b//bin/fble-disassemble.cov -I $::s/spec -m $::mpath
    }

    none {
      # Nothing to do.
    }

    default {
      puts "@FAILED"
      error "Unsupported @@fble-test@@ type: '$::type'"
    }
  }
}

if { [catch dispatch msg] } {
  puts "$path:$test_line: error: $type test failed:"
  puts "$msg"
  exit 1
}
puts $passed
exit 0

