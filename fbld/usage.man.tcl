# Usage fbld frontend for generating a man page.

# The following global variables are expected to be defined depending on the
# users backend of choice:
#   ::inline_<tag> - Process core inline tag @<tag>
#   ::block_<tag> - Process core block tag @<tag>

# @usage[INLINE name][INLINE brief][BLOCK content]
# Top level tag for usage documents.
# @param name  The name of the program.
# @param brief  A very brief description of the program.
# @param content  More usage and core tags describing the program.
#
# The following top level blocks are recommended for use in content in this
# order:
#   @synopsis
#   @description
#   @options
#   @exitstatus
#   @examples
#
# Each of these are treated as separate sections of the document.
proc block_usage {name brief content} {
  ::block_man 1 $name "@FbleVersion (@BuildStamp)" "@section\[NAME\]\[$name - $brief\]\n\n$content"
}

# @synopsis[INLINE text]
# Synopsis for how to run a command.
# @param text  Command synopsis.
proc block_synopsis {text} {
  ::block_section "SYNOPSIS" "@par\[$text\]"
}

# @description[BLOCK body]
# A description of the command.
# @param body  The text of the description.
proc block_description {body} {
  ::block_section "DESCRIPTION" $body
}

# @options[BLOCK body]
# A section describing the command line options.
# Use the @opt tag to describe an option.
# @param body  A description of the command line options.
proc block_options {body} {
  ::block_section "OPTIONS" $body
}

# @opt[INLINE opt][INLINE desc]
# Describes a command line option.
# @param opt  The option flags and arguments.
# @param desc  A description of the option.
proc block_opt {opt desc} {
  ::block_def $opt $desc
}

# @exitstatus[BLOCK body]
# A section describing the exit status of the command.
# @param body  A description of the exit status.
proc block_exitstatus {body} {
  ::block_section "EXIT STATUS" $body
}

# @examples[BLOCK body]
# A section giving example uses of the command.
# Use @ex to give individual examples.
# @param body  Example command line uses.
proc block_examples {body} {
  ::block_section "EXAMPLES" $body
}

# @ex[INLINE text][BLOCK desc]
# Gives an example command line use.
# @param text  The sample command line.
# @param desc  A description of the command line.
proc block_ex {text desc} {
  ::block_par "@l\[$text\]"
  ::block $desc
}
