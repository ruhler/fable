# fble-spec-test.tcl
#
# Helper script to run a spec test.
#
# Usage:
#   tclsh fble-test-spec.tcl FBLE
#
#   FBLE - Path to the fble test file, relative to spec/ directory.
#
# Example:
#   tclsh fble-test-spec.tcl SpecTests/00_Test/NoError.fble
#
# Parses the @@fble-test@@ metadata of spec/$FBLE and executes the test
# appropriately. Reports an error in case of test failure.
#
# This script assumes it is run from the top level directory, with access to
# fble binaries in the out directory and out/spec/${FBLE}.d the result of running
# fble-deps on the FBLE file.

set ::dir "spec"
set ::fble [lindex $argv 0]
set ::path $dir/$fble
set ::mpath "/[file rootname $fble]%"

# Parse the @@fble-test@@ metadata.
set type "none"
set loc ""
set fin [open "$path" "r"]
set test_line 1
while {[gets $fin line] >= 0} {
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

proc compile {} {
  # Parse the list of .fble files the test depends on from the .d file.
  set fbles [list]
  foreach x [lrange [split [read [open out/spec/$::fble.d "r"]]] 1 end] {
    switch $x {
      {} {}
      "\\" {}
      default { lappend fbles [string map { "spec/" "" } $x] }
    }
  }

  # TODO: compile each of the .fble files and link into an executable.
  puts $fbles
}

proc dispatch {} {
  switch $::type {
    no-error {
      # TODO: Run compiled too.
      compile
      exec out/fble/test/fble-test.cov --profile /dev/null -I spec -m $::mpath
      exec out/fble/bin/fble-disassemble.cov -I spec -m $::mpath
    }

    compile-error {
      set output [exec out/fble/test/fble-test.cov --compile-error -I spec -m $::mpath]
      if {-1 == [string first ":$::loc: error" $output]} {
        error "Expected error at $::loc, but got:\n$output"
      }
    }

    runtime-error {
      # TODO: Run compiled too.
      compile
      set output [exec out/fble/test/fble-test.cov --runtime-error -I spec -m $::mpath]
      if {-1 == [string first ":$::loc: error" $output]} {
        error "Expected error at $::loc, but got:\n$output"
      }
      exec out/fble/bin/fble-disassemble.cov -I spec -m $::mpath
    }
    
    memory-constant {
      # TODO: Run compiled too.
      compile
      exec out/fble/test/fble-mem-test.cov -I spec -m $::mpath
      exec out/fble/bin/fble-disassemble.cov -I spec -m $::mpath
    }

    memory-growth {
      # TODO: Run compiled too.
      compile
      exec out/fble/test/fble-mem-test.cov --growth -I spec -m $::mpath
      exec out/fble/bin/fble-disassemble.cov -I spec -m $::mpath
    }

    none {
      # Nothing to do.
    }

    default {
      error "Unsupported @@fble-test@@ type: '$type'"
    }
  }
}

if { [catch dispatch msg] } {
  puts "$path:$test_line: error: $type test failed:"
  puts "$msg"
  exit 1
}
exit 0

