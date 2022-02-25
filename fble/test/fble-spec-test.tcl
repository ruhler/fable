
# TODO:
# * Separate parsing from dispatch. Return <type> <msg> instead of calling
#   directly.
# * Implement test-* procedures.
# * Decide best way to specify dir/fble.
# * Grep for @@fble-test@@ first, before testing all the sub options, to save
#   performance? Or does that really not matter at all in practice?

set path [lindex $argv 0]

set dir "spec"
set fble [string range $path 5 end]
set mpath "/[file rootname $fble]%"

proc test-no-error {} {
  puts "no-error"
}

proc test-compile-error {msg} {
  puts "compile-error: $msg"
}

proc test-runtime-error {msg} {
  puts "runtime-error: $msg"
}

proc test-memory-constant {} {
  puts "memory-constant"
}

proc test-memory-growth {} {
  puts "memory-growth"
}

proc test-none {} {
  puts "none"
}

set fin [open "$dir/$fble" "r"]
while {[gets $fin line] >= 0} {
  if {1 == [regexp {@@fble-test@@ no-error} $line]} {
    test-no-error
    exit 0
  }

  if {1 == [regexp {@@fble-test@@ compile-error (.*)} $line fullmatch msg]} {
    test-compile-error $msg
    exit 0
  }

  if {1 == [regexp {@@fble-test@@ runtime-error (.*)} $line fullmatch msg]} {
    test-runtime-error $msg
    exit 0
  }

  if {1 == [regexp {@@fble-test@@ memory-constant} $line]} {
    test-memory-constant
    exit 0
  }

  if {1 == [regexp {@@fble-test@@ memory-growth} $line]} {
    test-memory-growth
    exit 0
  }

  if {1 == [regexp {@@fble-test@@} $line]} {
    throw "invalid fble-test attribute: $line" 
  }
}

test-none
exit 0

