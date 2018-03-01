
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

source build.fblc.tcl
source build.fbld.tcl

puts "fblc Coverage: "
puts "  Spec: [exec tail -n 1 out/fblc/cov/spec/fblc.gcov]"
puts "  All : [exec tail -n 1 out/fblc/cov/all/fblc.gcov]"
puts "fbld Coverage: "
puts "  Spec: [exec tail -n 1 out/fbld/cov/spec/fbld.gcov]"
puts "  All : [exec tail -n 1 out/fbld/cov/all/fbld.gcov]"

if {[llength $::failed] != 0} {
  puts "Failed Tests:"
  foreach test $::failed {
    puts "  $test"
  }

  exit 1
}
