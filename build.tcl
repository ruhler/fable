
# Run a command, printing the command as it is run.
proc run {args} {
  puts $args
  exec {*}$args
}

set ::failed [list]
proc fail {name msg} {
  puts stderr $msg
  lappend ::failed $name
}

set ::num_tests_failed 0
set ::num_tests_passed 0

# Returns the given file name relative to the current working directory.
proc relative_file {path} {
  set cwd "[file normalize [pwd]]/"
  set fname [file normalize $path]
  return [string range $fname [string length $cwd] [string length $fname]]
}

proc testl {loc args} {
  set line [dict get $loc line]
  set file [dict get $loc file]
  set name "[relative_file $file]:$line"

  try {
    puts "test $name"
    eval $args
  } on error {results options} {
    fail $name "$file:$line: error:\n$results"
    incr ::num_tests_failed
    return
  }
  incr ::num_tests_passed
}

proc test {args} {
  testl [info frame -1] {*}$args
}

source build.fble.tcl

puts ""
puts "Fble Coverage: "
puts "  Spec: [exec tail -n 1 out/cov/spec/fble.gcov]"
puts "  All : [exec tail -n 1 out/cov/all/fble.gcov]"
puts ""
if {[llength $::failed] != 0} {
  puts "Failed Tests:"
  foreach test $::failed {
    puts "  $test"
  }
  puts ""
}

puts "Test Summary:"
puts "  $::num_tests_passed passed, $::num_tests_failed failed, [expr $::num_tests_passed + $::num_tests_failed] total"
puts ""

if {[llength $::failed] != 0} {
  exit 1
}
