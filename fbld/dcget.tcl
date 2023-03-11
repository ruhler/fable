# Extracts a document comment from a C file.
#
# Usage: tclsh8.6 dcget.tcl ID < FILE
#  Where ID is the name of an identifier whose doc comment to extract.
#    and FILE is a C header or implementation file.
#
# Doc comments are fbld formatted comments of the form:
# /**
#  * @func[ID] ...
#  * ...
#  * ...
#  */
#
# Which is extract as:
# @func[ID] ...
# ...
# ...

set id [lindex $argv 0]
set lines [split [read stdin] "\n"]
set i 0

while {[string first " * @func\[$id\]" [lindex $lines $i]] != 0} {
  incr i
}

puts [string range [lindex $lines $i] 3 end]
incr i

while {[string first " * " "[lindex $lines $i] "] == 0} {
  puts [string range [lindex $lines $i] 3 end]
  incr i
}
