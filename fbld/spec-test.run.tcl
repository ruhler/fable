# spec-test.run.tcl
#
# Helper script to run an fbld spec test.
#
# Usage:
#   tclsh8.6 fbld SpecTests.fbld FBLD
#
#   fbld - Path to the fbld binary to test.
#   SpecTests.fbld - Path to the SpecTests.fbld library.
#   FBLD - Path to the fbld test file.
#
# Example:
#
#   tclsh8.6 spec-test.run.tcl out/pkgs/fbld/bin/fbld fbld/SpecTests.fbld fbld/SpecTests/basic.fbld
#
# Parses the @test metadata of $FBLD and executes the test appropriately.
# Reports an error in case of test failure.
#
# Test metadata should be the following to indicate no error is expected:
#
#   @test no-error @@
#
# Test metadata should be the following to indicate an error is expected at
# the given line and column numbers:
#
#   @test error 3:10 @@

set ::fbld [lindex $argv 0]
set ::lib [lindex $argv 1]
set ::test [lindex $argv 2]

# Parse the @test metadata.
set type "???"
set loc ""
set fin [open "$::test" "r"]
set test_line 1
while {[gets $fin line] >= 0} {
  set first [string first "@test " $line]
  if {$first != -1} {
    set index [expr $first + [string length "@test "]]
    set metadata [string trim [string range $line $index end-3]]
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

proc execv { args } {
  puts $args
  exec {*}$args
}

# expect_error
#   Helper function for expected error case.
#
# Inputs:
#   loc - location of expected error.
#   args - command to run.
proc expect_error { loc args } {
  set output ""
  set status 0
  try {
    execv {*}$args 2>@1
  } trap CHILDSTATUS {results options} {
    set output $results
    set status [lindex [dict get $options -errorcode] 2]
  }

  if {$status == 0} {
    error "Expected $type error, but no error encountered."
  }

  if {-1 == [string first ":$::loc: error" $output]} {
    error "Expected error at $::loc, but got:\n$output"
  }
}

proc dispatch {} {
  switch $::type {
    no-error {
      execv $::fbld $::lib $::test
    }

    error {
      expect_error $::loc $::fbld $::lib $::test
    }

    default {
      error "Unsupported @test type: '$::type'"
    }
  }
}

if { [catch dispatch msg] } {
  puts "$::test:$test_line: error: $type test failed:"
  puts "$msg"
  exit 1
}
exit 0
