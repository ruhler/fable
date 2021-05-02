# extract-test-spec.tcl
#
# A script to extract the .fble files from an individual fble language
# specification test.
#
# See langs/fble/README.txt for a description of how the language
# specification tests are specified.
#
# Usage:
#   tclsh extract-test-spec.tcl test.tcl dir Nat.fble
#
#   test.tcl - The specification test to extract.
#   dir - The directory to extract the spec test to.
#   Nat.fble - the Nat% testing module.
#
# Extracts .fble files so that the main .fble file for the test is at
# $dir/test.fble and any required modules are available in the search path
# $dir.

set ::testtcl [lindex $argv 0]
set ::dir [lindex $argv 1]
set ::natfble [lindex $argv 2]

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

proc fble-test-extract { expr modules } {
  set fprgm $::dir/test.fble
  exec echo $expr > $fprgm
  exec ln -f $::natfble $::dir/Nat.fble
  write_modules $::dir $modules
}

proc fble-test { expr args } {
  fble-test-extract $expr $args
}

proc fble-test-error { loc expr args } {
  fble-test-extract $expr $args
}

# See langs/fble/README.txt for the description of this function
proc fble-test-memory-constant { expr } {
  fble-test-extract $expr {}
}

# See langs/fble/README.txt for the description of this function
proc fble-test-memory-growth { expr } {
  fble-test-extract $expr {}
}

source $::testtcl
