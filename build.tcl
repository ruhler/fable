
# Run a command, printing the command as it is run.
proc run {args} {
  puts $args
  exec {*}$args
}

set ::skipped [list]
proc skip { args } {
  set testloc [info frame -1]
  set line [dict get $testloc line]
  set file [dict get $testloc file]
  set name "[file tail $file]_$line"
  lappend ::skipped $name
}

source build.fblc.tcl
source build.fbld.tcl

puts "Skipped Tests:"
foreach test $::skipped {
  puts "  $test"
}
puts "fblc Coverage: "
puts "  Spec: [exec tail -n 1 out/fblc/cov/spec/fblc.gcov]"
puts "  All : [exec tail -n 1 out/fblc/cov/all/fblc.gcov]"
puts "fbld Coverage: "
puts "  Spec: [exec tail -n 1 out/fbld/cov/spec/fbld.gcov]"
puts "  All : [exec tail -n 1 out/fbld/cov/all/fbld.gcov]"

