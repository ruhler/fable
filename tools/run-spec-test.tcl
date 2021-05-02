# run-test-spec.tcl
#
# Helper script to run a spec test.
#
# Usage:
#   tclsh run-test-spec.tcl test.tcl cmd...
#
#   test.tcl - The spec test file to run.
#   cmd... - A command to run to execute the spec test.
#
# Runs the given command to execute the test, asserts that the expected
# substring is in the test output if appropriate according to the test
# specification, and reports an error with a pointer to test.tcl in case of
# failure.

set ::testtcl [lindex $argv 0]
set ::command [lrange $argv 1 end]

# For non-error style tests, we just run the test normally.
proc test {} { set output [exec {*}$::command] }
proc fble-test { expr args } { test }
proc fble-test-memory-constant { expr } { test }
proc fble-test-memory-growth { expr } { test }

# Otherwise run the command and search for the error string.
proc fble-test-error { loc expr args } {
  set output [exec {*}$::command]
  if {-1 == [string first ":$loc: error" $output]} {
    error "Expected error at $loc, but got:\n$output"
  }
}

if { [catch {source $::testtcl} msg] } {
  puts "$msg"
  puts "$::testtcl:1: error: test failed"
  exit 1
}
