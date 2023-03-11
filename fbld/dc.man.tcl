# Doc Comment Fbld frontend for generating a man page.

# An fbld block invoke function that sets ::dc_man_func_args and
# ::dc_man_func_return based on contents of an @func tag.
proc dc_get_args_and_return { name args } {
  switch -exact $name {
    arg {
      set type [lindex $args 0]
      set name [lindex $args 1]
      lappend ::dc_man_func_args [string trim $type]
      lappend ::dc_man_func_args [string trim $name]
    }

    returns {
      set ::dc_man_func_return [lindex $args 0]
    }

    default {}
  }
}

# @func[INLINE name][INLINE brief][BLOCK content]
# Top level tag for a function doc comment.
# @param name The name of the function.
# @param brief A very brief description of the function.
# @param content description of function.
#
# The content should have the following structure:
# 1. general description.
# 2. args using @arg commands.
# 3. return type using @returns command.
# 4. side effects using @sideeffects command.
proc block_func {name brief content} {
  set ::dc_man_func_args [list]
  set ::dc_man_func_return "void"
  ::fbld::block dc_get_args_and_return $content

  set body "@section\[NAME\]\[$name - $brief\]\n\n"
  append body "@section\[SYNOPSYS\]\[@par\[@l\[#include <fble/fble.h>\]\]\n@par\["
  append body "@l\[$::dc_man_func_return $name\]@l\[(\]"
  set comma ""
  foreach { t n } $::dc_man_func_args {
    append body "$comma@l\[$t\] @a\[$n\]"
    set comma "@l\[,\] "
  }
  append body "@l\[);\]\]\]\n\n"
  append body "@section\[DESCRIPTION\]\[$brief\n\n$content\]"
  ::block_man 3 $name $body
}

# @arg[INLINE type][INLINE name][INLINE description]
proc block_arg {type name desc} {
  ::block_def "@l\[[string trim $type]\] @a\[[string trim $name]\]" $desc
}

# @returns[INLINE type][BLOCK description]
proc block_returns {type desc} {
  ::block_section "RETURN VALUE" "@definition\[@l\[$type\]\]\[$desc\]"
}

# @sideeffects[BLOCK description]
proc block_sideeffects {desc} {
  ::block_section "SIDE EFFECTS" $desc
}

# @i - list item that supports wrapping from same line.
proc block_i { first args } {
  ::block_item "$first [join $args " "]"
}
