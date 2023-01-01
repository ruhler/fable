# Usage fbld frontend.
#
# Used for describing how to use a program.
# Generates:
#   Section 1 man pages.
#   Program help text.

set fblddir [file dirname [file dirname [file normalize [info script]]]]
source $fblddir/fbld.tcl
source $fblddir/core.tcl

namespace import fbld/core::inline_*
namespace import fbld/core::block_*

# @usage[INLINE name][INLINE brief][BLOCK content]
# Top level tag for usage documents.
# @param name  The name of the program.
# @param brief  A very brief description of the program.
# @param content  More usage and core tags describing the program.
#
# The following top level blocks are recommended for use in content in this
# order:
#   @synopsys
#   @description
#   @options
#   @exitstatus
#   @examples
#
# Each of these are treated as separate sections of the document.
proc block_usage {name brief content} {
  ::fbld::inline invoke_inline $name
  ::fbld::inline invoke_inline $brief
  ::fbld::block invoke_block $content
}

# @synopsys[INLINE text]
# Synopsys for how to run a command.
# @param text  Command synopsys.
proc block_synopsys {text} {
  ::fbld::inline invoke_inline $text
}

# @description[BLOCK body]
# A description of the command.
# @param body  The text of the description.
proc block_description {body} {
  ::fbld::block invoke_block $body
}

# @options[BLOCK body]
# A section describing the command line options.
# Use the @opt tag to describe an option.
# @param body  A description of the command line options.
proc block_options {body} {
  ::fbld::block invoke_block $body
}

# @opt[INLINE opt][INLINE desc]
# Describes a command line option.
# @param opt  The option flags and arguments.
# @param desc  A description of the option.
proc block_opt {opt desc} {
  ::fbld::inline invoke_inline $opt
  ::fbld::inline invoke_inline $desc
}

# @exitstatus[BLOCK body]
# A section describing the exit status of the command.
# @param body  A description of the exit status.
proc block_exitstatus {body} {
  ::fbld::block invoke_block $body
}

# @examples[BLOCK body]
# A section giving example uses of the command.
# Use @ex to give individual examples.
# @param body  Example command line uses.
proc block_examples {body} {
  ::fbld::block invoke_block $body
}

# @ex[INLINE text][BLOCK desc]
# Gives an example command line use.
# @param text  The sample command line.
# @param desc  A description of the command line.
proc block_ex {text desc} {
  ::fbld::inline invoke_inline $text
  ::fbld::block invoke_block $desc
}

proc ::invoke_inline {cmd args} { inline_$cmd {*}$args }
proc ::invoke_block {cmd args} { block_$cmd {*}$args }

set input [lindex $argv 0]
::fbld::block invoke_block [read [open $input "r"]]

