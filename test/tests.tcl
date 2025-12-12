# Summarize test results.
#
# Usage: tclsh8.6 tests.tcl
#
# Test results are read from stdin using the following protocol:
# * @[<name>] identifies a test.
# * @PASS says the previously named test passed.
# * @FAIL says the previously named test failed.
# * @EXPECTED_FAIL says the previously named test failed as expected.
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

set ::pass 0
set ::fail 0
set ::expected_fail 0

# process --
#   Given a string with the contents of a test, including test name and
#   status, process it.
proc process { test } {
  if {[regexp {@\[(.*)\]} $test _ name]} {
    if {[string first "@FAIL" $test] != -1} {
      incr ::fail
      puts "$test\n"
    } elseif {[string first "@EXPECTED_FAIL" $test] != -1} {
      incr ::expected_fail
    } elseif {[string first "@PASS" $test] != -1} {
      incr ::pass
    } else {
      incr ::fail
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

set total [expr $::pass + $::expected_fail + $::fail]
puts -nonewline "Test Summary:"
if {$::fail == 0} {
  puts " PASS"
} else {
  puts " FAIL"
}

if {$::pass > 0} {
  puts "  passed:              $::pass"
}

if {$::expected_fail > 0} {
  puts "  expected failures:   $::expected_fail"
}

if {$::fail > 0} {
  puts "**unexpected failures: $::fail**"
}

puts "  total:                 $total"

if {$::fail != 0} {
  exit 1
}
