# Summarize the status of tests.
#
# Usage: tclsh tests.tcl test1.tr test2.tr, ...
#
# A file test1.tr describes the results of the test named 'test1'. The last
# line of the file should be 'PASSED' for passing tests and 'FAILED' for
# failing tests. Any error messages should be printed in previous lines of the
# file.
#
# A full test summary with passing and failing tests is printed to stdout.
# A limited test summary with just failing tests and totals is printed to
# stderr.
#
# Exit code is 0 if all tests are passing, 1 otherwise.

set passed 0
set failed 0
set failures [list]

foreach t $argv {
  set lines [split [string trim [read [open $t]]] "\n"]
  set message [join [lrange $lines 0 end-1] "\n"]
  set status [lindex $lines end]
  set name [file rootname $t]
  if {$status == "PASSED"} {
    incr passed
    puts "$name: PASSED"
  } elseif {$status == "FAILED"} {
    incr failed
    lappend failures $name
    puts "$name: FAILED\n$message"
    puts stderr "$name: FAILED\n$message"
  } else {
    error "$t:1:0: error: invalid test status '$status'"
  }
}

if {$failed != 0} {
  puts "Failed Tests:"
  puts stderr "Failed Tests:"
  foreach test $::failures {
    puts "  $test"
    puts stderr "  $test"
  }
  puts ""
  puts stderr ""
}

puts "Test Summary:"
puts "  $passed passed, $failed failed, [expr $passed + $failed] total"
puts ""
puts stderr "Test Summary:"
puts stderr "  $passed passed, $failed failed, [expr $passed + $failed] total"
puts stderr ""

if {$failed != 0} {
  exit 1
}
