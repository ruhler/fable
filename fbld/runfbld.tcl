# A generic fbld front end.
#
# Usage: tclsh8.6 runfbld.tcl [SCRIPT...] < foo.fbld
# Where SCRIPT is an fbld tcl script that defines inline_* and block_* procs.

set fblddir [file dirname [file normalize [info script]]]
source $fblddir/fbld.tcl

foreach arg $argv {
  source $arg
}

proc ::invoke_inline {cmd args} { inline_$cmd {*}$args }
proc ::invoke_block {cmd args} { block_$cmd {*}$args }

proc ::unescape {text} { ::fbld::unescape $text }
proc ::inline {text} { ::fbld::inline invoke_inline $text }
proc ::block {text} { ::fbld::block invoke_block $text }
proc ::output {str} { puts -nonewline $str }

::block [read stdin]
