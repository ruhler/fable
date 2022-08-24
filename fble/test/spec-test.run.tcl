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

proc module_path { path } {
  # Add quotes around module path words so we can name spec tests to match
  # spec section numbers, e.g. 2.2-Kinds.
  return "/\'[string map { "/" "\'/\'"} [file rootname $path]]\'%"
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
    compile { set es 1 }
    runtime { set es 2 }
    default { error "unsupported error type: $type" }
  }

  set output ""
  set status 0
  try {
    exec {*}$args 2>@1
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

set ::dir "spec"
set ::fble [lindex $argv 0]
set ::path $dir/$fble
set ::mpath [module_path $fble]

puts "@\[$fble\]"

# Directory to use for any artifacts from the test run.
set ::outdir out/$dir/[file rootname $fble]
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
    set m [module_path $fble]
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
      expect_error compile $::loc out/fble/test/fble-test.cov -I spec -m $::mpath
    }

    runtime-error {
      expect_error runtime $::loc out/fble/test/fble-test.cov -I spec -m $::mpath

      compile FbleTestMain
      expect_error runtime $::loc $::outdir/compiled

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

