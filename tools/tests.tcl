# Summarize the status of tests.
#
# Usage: tclsh tests.tcl tests.txt
#
# The file tests.txt should contain a whitespace separated list of *.tr test
# result files. A foo.tr test result file describes the results of the test
# named 'foo'. The contents of the file should be 'PASSED' for passing tests
# and 'FAILED' for failing tests.
#
# A test summary is printed to stdout.
# Exit code is 0 if all tests are passing, 1 otherwise.

set passed 0
set failed 0
set failures [list]

set tests [string trim [read [open [lindex $argv 0]]]]
foreach t $tests {
  set status [string trim [read [open $t]]]
  set name [file rootname $t]
  if {$status == "PASSED"} {
    incr passed
  } elseif {$status == "FAILED"} {
    incr failed
    lappend failures $name
  } else {
    error "$t:1:0: error: invalid status for test $t: '$status'"
  }
}

if {$failed != 0} {
  puts "Failed Tests:"
  foreach test $::failures {
    puts "  $test"
  }
  puts ""
}

puts "Test Summary: $passed passed, $failed failed, [expr $passed + $failed] total"

if {$failed != 0} {
  exit 1
}
