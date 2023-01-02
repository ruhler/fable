# Tutorial fbld frontend.
#
# Used for describing tutorials.
#
# Usage: tclsh8.6 tutorial.tcl [--html] < foo.fbld
#
#  --html generates html output to stdout.

set fblddir [file dirname [file dirname [file normalize [info script]]]]
source $fblddir/fbld.tcl
source $fblddir/core.tcl
source $fblddir/backends/html.tcl

# @tutorial[INLINE name][BLOCK content]
# Top level tag for tutorials.
# @param name  The name of the tutorial.
# @param content  The tutorial content.
proc block_tutorial {name content} {
  switch -exact $::mode {
    check {
      ::fbld::inline invoke_inline $name
      ::fbld::block invoke_block $content
    }
    html {
      ::fbld::block invoke_block $content
    }
  }
}

# @exercise[BLOCK content]
# An exercise for the reader.
# @param content  A description of the exercise.
proc block_exercise {content} {
  ::fbld::block invoke_block $content
}

proc ::invoke_inline {cmd args} { inline_$cmd {*}$args }
proc ::invoke_block {cmd args} { block_$cmd {*}$args }

set html "false"

foreach arg $argv {
  switch -exact $arg {
    --html { set html "true" }
    default { error "Unexpected command line argument: $arg" }
  }
}

set input [read stdin]

set ::mode check
namespace import fbld/core::inline_* fbld/core::block_*
::fbld::block invoke_block $input
namespace forget fbld/core::inline_* fbld/core::block_*

if $html {
  set ::mode html
  namespace import fbld/html::inline_* fbld/html::block_*
  proc ::output {str} { puts -nonewline $str }
  ::fbld::block invoke_block $input
  namespace forget fbld/html::inline_* fbld/html::block_*
}
