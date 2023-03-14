# Tutorial fbld frontend.
#
# Used for describing tutorials.

# The following global variables are expected to be defined:
#   ::inline_<tag> - Process core inline tag @<tag>
#   ::block_<tag> - Process core block tag @<tag>

# @tutorial[INLINE name][BLOCK content]
# Top level tag for tutorials.
# @param name  The name of the tutorial.
# @param content  The tutorial content.
proc block_tutorial {name content} {
  ::block_doc $name " @FbleVersion (@BuildStamp)\n\n$content"
}

# @exercise[BLOCK content]
# An exercise for the reader.
# @param content  A description of the exercise.
proc block_exercise {content} {
  ::block_ $content
}
