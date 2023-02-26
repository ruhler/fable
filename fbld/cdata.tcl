# Outputs a C header file with the definition of an array containing the bytes
# of a given file.
#
# Usage: tclsh8.6 cdata.tcl NAME < foo.txt > foo.txt.h
# 
# Where NAME is the name of an unsigned char[] to define containing the bytes
# of the file.

set name [lindex $argv 0]
set text [read stdin]

puts -nonewline "static const unsigned char $name\[\] = \{";
set c 80
set i 0
set len [string length $text]
while {$i < $len} {
  if {$c > 70} {
    puts -nonewline "\n  "
    set c 2
  }
  set code "[scan [string index $text $i] %c], "
  puts -nonewline $code
  incr c [string length $code]
  incr i
}
puts "'\\0'\n\};"
