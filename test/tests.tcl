# Summarize test results.
#
# Usage: tclsh tests.tcl
#
# Test results are read from stdin using the following protocol:
# * @[<name>] identifies a test.
# * @PASSED says the previously named test passed.
# * @FAILED says the previously named test failed.
# * @XFAILED says the previously named test failed as expected.
# * Output for a test is everything between @[...] and the next test.
# * If there is no status between @[...] and the next test, the test is
#   considered as failing.
#
# The @[...] string should be at the beginning of a line. The test status can
# be anywhere in a line.
#
# This script outputs the list of failed tests and a summary of the number of
# tests run, passed, failed, etc. Exit status is 0 if all tests pass, non-zero
# otherwise.

set ::passed 0
set ::xfailed 0
set ::failed 0

# process --
#   Given a string with the contents of a test, including test name and
#   status, process it.
proc process { test } {
  if {[regexp {@\[(.*)\]} $test _ name]} {
    if {[string first "@FAILED" $test] != -1} {
      incr ::failed
      puts "$test\n"
    } elseif {[string first "@XFAILED" $test] != -1} {
      incr ::xfailed
    } elseif {[string first "@PASSED" $test] != -1} {
      incr ::passed
    } else {
      incr ::failed
      puts "$test\n"
    }
  }
}

set lines [list]
while {[gets stdin line] >= 0} {
  if {[string match {@\[*\]*} $line]} {
    process [join $lines "\n"]
    set lines [list]
  }
  lappend lines $line
}
process [join $lines "\n"]

set total [expr $::passed + $::xfailed + $::failed]
puts "Test Summary: $::passed passed, $::xfailed xfailed, $::failed failed, $total total"

if {$::failed != 0} {
  exit 1
}
