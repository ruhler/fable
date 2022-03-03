# spec-test.run.tcl
#
# Helper script to run a spec test.
#
# Usage:
#   tclsh test-spec.tcl FBLE
#
#   FBLE - Path to the fble test file, relative to spec/ directory.
#
# Example:
#
#   tclsh spec-test.run.tcl SpecTests/00_Test/NoError.fble
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

# Directory to use for any artifacts from the test run.
set ::outdir out/$dir/[file rootname $fble]
exec mkdir -p $outdir

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

# compile -- 
#   Compiles the test .fble file into an $outdir/compiled binary.
#
# Inputs:
#   main - FbleTestMain or FbleMemTestMain.
proc compile {main} {
  # Parse the list of .fble files the test depends on from the .d file.
  set fbles [list]
  foreach x [lrange [split [read [open out/spec/$::fble.d "r"]]] 1 end] {
    switch $x {
      {} {}
      "\\" {}
      default { lappend fbles [string map { "spec/" "" } $x] }
    }
  }

  set objs [list]
  foreach fble $fbles {
    set name [string map { "/" "-" } $fble]
    set m "/[file rootname $fble]%"
    set asm $::outdir/$name.s
    set obj $::outdir/$name.o
    set flags [list]
    if {$fble == $::fble} {
      lappend flags "--main"
      lappend flags $main
    }

    exec out/fble/bin/fble-compile.cov {*}$flags -c -I $::dir -m $m > $asm
    exec as -o $obj $asm
    lappend objs $obj
  }

  lappend objs out/fble/test/libfbletest.a
  lappend objs out/fble/lib/libfble.a
  exec gcc -pedantic -Wall -Werror -gdwarf-3 -ggdb -no-pie -O3 -o \
    $::outdir/compiled {*}$objs
}

proc dispatch {} {
  switch $::type {
    no-error {
      exec out/fble/test/fble-test.cov --profile /dev/null -I spec -m $::mpath

      compile FbleTestMain
      exec $::outdir/compiled --profile /dev/null

      exec out/fble/bin/fble-disassemble.cov -I spec -m $::mpath
    }

    compile-error {
      set output [exec out/fble/test/fble-test.cov --compile-error -I spec -m $::mpath]
      if {-1 == [string first ":$::loc: error" $output]} {
        error "Expected error at $::loc, but got:\n$output"
      }
    }

    runtime-error {
      set output [exec out/fble/test/fble-test.cov --runtime-error -I spec -m $::mpath]
      if {-1 == [string first ":$::loc: error" $output]} {
        error "Expected error at $::loc, but got:\n$output"
      }

      compile FbleTestMain
      set output [exec $::outdir/compiled --runtime-error]
      if {-1 == [string first ":$::loc: error" $output]} {
        error "Expected error at $::loc, but got:\n$output"
      }

      exec out/fble/bin/fble-disassemble.cov -I spec -m $::mpath
    }
    
    memory-constant {
      exec out/fble/test/fble-mem-test.cov -I spec -m $::mpath

      compile FbleMemTestMain
      exec $::outdir/compiled

      exec out/fble/bin/fble-disassemble.cov -I spec -m $::mpath
    }

    memory-growth {
      exec out/fble/test/fble-mem-test.cov --growth -I spec -m $::mpath

      compile FbleMemTestMain
      exec $::outdir/compiled --growth

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

