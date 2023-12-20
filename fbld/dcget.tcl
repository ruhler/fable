# Extracts a document comment from a C file.
#
# Usage: tclsh8.6 dcget.tcl ID < FILE
#  Where ID is the name of an identifier whose doc comment to extract.
#    and FILE is a C header or implementation file.
#
#  If no ID is provided, all doc comments are extracted.
#
# Doc comments are fbld formatted comments of the form:
# /**
#  * @func[ID] ...
#  * ...
#  * ...
#  */
#
# Which is extracted as:
# @func[ID] ...
# ...
# ...

lappend argv "*"
set id [lindex $argv 0]
set input_text [read stdin]
set lines [split $input_text "\n"]

set in_doc_comment false
set found [string equal $id "*"]
foreach line [split $input_text "\n"] {
  if {[string first " * " "$line "] != 0} {
    set in_doc_comment false
  }

  if {[string match " \* @func\\\[$id\\\]*" $line] == 1} {
    set in_doc_comment true
    set found true
  }

  if {[string match " \* @func $id" $line] == 1} {
    set in_doc_comment true
    set found true
  }

  if {[string match " \* @file\\\[$id\\\]*" $line] == 1} {
    set in_doc_comment true
    set found true
  }

  if {[string match " \* @file $id*" $line] == 1} {
    set in_doc_comment true
    set found true
  }

  if {$in_doc_comment} {
    puts [string range $line 3 end]
  }
}

if {!$found} {
  error "$id not found"
}
