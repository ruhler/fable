# fble-spec-test.tcl
#
# Helper script to run a spec test.
#
# Usage:
#   tclsh fble-test-spec.tcl dir path
#
#   dir - The fble -I directory for the test.
#   fble - Path to the fble test file, relative to dir.
#
# Example:
#   tclsh fble-test-spec.tcl spec SpecTests/00_Test/NoError.fble
#
# Parses the @@fble-test@@ metadata of $dir/$fble and executes the test
# appropriately. Reports an error in case of test failure.
#
# This script assumes it is run from the top level directory, with access to
# fble binaries in the out directory.

set ::dir [lindex $argv 0]
set ::fble [lindex $argv 1]
set ::path $dir/$fble
set ::mpath "/[file rootname $fble]%"

# parse-test-metadata
#   Parses the test metadata from a .fble file.
#
# Inputs:
#   path - the full path to the .fble file.
#   typevar - the name of a variable to set with the parsed metadata type.
#   locvar - the name of a variable to set with the parsed metadata message.
#
# The type variable is set to none if no @@fble-test@@ meta data is present.
proc parse-test-metadata { path typevar locvar } {
  upvar $typevar type
  upvar $locvar loc

  set fin [open "$path" "r"]
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
      close $fin
      return
    }
  }
  set type "none"
  set loc ""
  close $fin
  return
}

parse-test-metadata $path type loc

switch $type {
  no-error {
    # TODO: Run compiled too.
    exec out/fble/test/fble-test.cov --profile /dev/null -I spec -m $::mpath
    exec out/fble/bin/fble-disassemble.cov -I spec -m $::mpath
  }

  compile-error {
    set output [exec out/fble/test/fble-test.cov --compile-error -I spec -m $::mpath]
    if {-1 == [string first ":$loc: error" $output]} {
      error "Expected error at $loc, but got:\n$output"
    }
  }

  runtime-error {
    # TODO: Run compiled too.
    set output [exec out/fble/test/fble-test.cov --runtime-error -I spec -m $::mpath]
    if {-1 == [string first ":$loc: error" $output]} {
      error "Expected error at $loc, but got:\n$output"
    }
    exec out/fble/bin/fble-disassemble.cov -I spec -m $::mpath
  }
  
  memory-constant {
    # TODO: Run compiled too.
    exec out/fble/test/fble-mem-test.cov -I spec -m $::mpath
    exec out/fble/bin/fble-disassemble.cov -I spec -m $::mpath
  }

  memory-growth {
    # TODO: Run compiled too.
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
