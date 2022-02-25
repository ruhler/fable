
# TODO:
# * Implement test-* procedures.
# * Decide best way to specify dir/fble.

set path [lindex $argv 0]

set dir "spec"
set fble [string range $path 5 end]
set mpath "/[file rootname $fble]%"

# parse-test-metadata
#   Parses the test metadata from a .fble file.
#
# Inputs:
#   path - the full path to the .fble file.
#   typevar - the name of a variable to set with the parsed metadata type.
#   msgvar - the name of a variable to set with the parsed metadata message.
#
# The type variable is set to none if no @@fble-test@@ meta data is present.
proc parse-test-metadata { path typevar msgvar } {
  upvar $typevar type
  upvar $msgvar msg

  set fin [open "$path" "r"]
  while {[gets $fin line] >= 0} {
    set first [string first "@@fble-test@@" $line]
    if {$first != -1} {
      set index [expr $first + [string length "@@fble-test@@"]]
      set metadata [string trim [string range $line $index end]]
      set type $metadata
      set msg ""
      set space [string first " " $metadata]
      if {$space != -1} {
        set type [string trim [string range $metadata 0 $space]]
        set msg [string trim [string range $metadata $space end]]
      }
      close $fin
      return
    }
  }
  set type "none"
  set msg ""
  close $fin
  return
}

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


parse-test-metadata $path type msg
puts "type='$type', msg='$msg'"

