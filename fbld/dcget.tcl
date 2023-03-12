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
set input_text [read stdin]

set start [string first " * @func\[$id\]" $input_text]
if {$start == -1} {
  error "@func\[$id\] not found"
}

set lines [split [string range $input_text $start end] "\n"]
set i 0

while {[string first " * " "[lindex $lines $i] "] == 0} {
  puts [string range [lindex $lines $i] 3 end]
  incr i
}
