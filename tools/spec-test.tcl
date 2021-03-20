# test-spec.tcl
#
# A script to run an individual fble language specification test.
#
# See langs/fble/README.txt for a description of how the language
# specification tests are specified.
#
# Usage:
#   tclsh test-spec.tcl fble-test fble-mem-test Nat.fble dir test.tcl
#
#   fble-test - the fble-test tool.
#   fble-mem-test - the fble-mem-test tool.
#   Nat.fble - the Nat% testing module.
#   dir - A directory unique to this test to use for intermediate files.
#   test.tcl - The language specification test to run.
#
# Runs the test, generating intermediate files to the given directory.
# Exits with status 0 if the test passes, non-zero if the test fails.
# Error messages will be printed to stdout.

set ::fbletest [lindex $argv 0]
set ::fblememtest [lindex $argv 1]
set ::natfble [lindex $argv 2]
set ::dir [lindex $argv 3]
set ::testtcl [lindex $argv 4]

proc write_modules { dir modules } {
  foreach m $modules {
    set name [lindex $m 0]
    set value [lindex $m 1]
    set submodules [lrange $m 2 end]
    exec echo $value > $dir/$name.fble
    exec mkdir -p $dir/$name
    write_modules $dir/$name $submodules
  }
}

proc fble-test-run { cmd expr modules } {
  set fprgm $::dir/test.fble
  exec echo $expr > $fprgm
  exec ln -f $::natfble $::dir/Nat.fble
  write_modules $::dir $modules
  exec {*}$cmd $fprgm $::dir
}

# See langs/fble/README.txt for the description of this function
proc fble-test { expr args } {
  # We test with profiling turned on to cover the profiling logic, which
  # depends heavily on the program being run.
  fble-test-run "$::fbletest --profile" $expr $args
}

# See langs/fble/README.txt for the description of this function
proc fble-test-error { loc expr args } {
  set fprgm $::dir/test.fble
  exec echo $expr > $fprgm
  write_modules $::dir $args

  set errtext [exec $::fbletest --error $fprgm $::dir]
  exec echo $errtext > $::dir/test.err
  if {-1 == [string first ":$loc: error" $errtext]} {
    error "Expected error at $loc, but got:\n$errtext"
  }
}

# See langs/fble/README.txt for the description of this function
proc fble-test-memory-constant { expr } {
  fble-test-run $::fblememtest $expr {}
}

# See langs/fble/README.txt for the description of this function
proc fble-test-memory-growth { expr } {
  fble-test-run "$::fblememtest --growth" $expr {}
}

# Output a message that makes it easy to jump to the test from logs if it
# fails.
puts "$::testtcl:1:0"
source $::testtcl
