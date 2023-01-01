# Tutorial fbld frontend.
#
# Used for describing tutorials.

set fblddir [file dirname [file dirname [file normalize [info script]]]]
source $fblddir/fbld.tcl
source $fblddir/core.tcl

namespace import fbld/core::inline_*
namespace import fbld/core::block_*

# @tutorial[INLINE name][BLOCK content]
# Top level tag for tutorials.
# @param name  The name of the tutorial.
# @param content  The tutorial content.
proc block_tutorial {name content} {
  ::fbld::inline invoke_inline $name
  ::fbld::block invoke_block $content
}

# @exercise[BLOCK content]
# An exercise for the reader.
# @param content  A description of the exercise.
proc block_exercise {content} {
  ::fbld::inline invoke_inline $content
}

proc ::invoke_inline {cmd args} { inline_$cmd {*}$args }
proc ::invoke_block {cmd args} { block_$cmd {*}$args }

set input [lindex $argv 0]
::fbld::block invoke_block [read [open $input "r"]]
