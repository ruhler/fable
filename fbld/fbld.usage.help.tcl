
# Converts fbld formatted, usage structured document to help text.
#
# Usage: tclsh8.6 fbld.usage.help.tcl usage.fbld > usage.h
#
# The output is in a format of a C header file that defines a string literal
# called fbldUsageHelpText.
set fblddir [file dirname [file normalize [info script]]]
source $fblddir/fbld.tcl

proc inline_ {text} { ::output [::fbld::unescape $text] }
proc inline_a {text} { inline_ $text }
proc inline_l {text} { inline_ $text }
proc inline_invoke {cmd args} { inline_$cmd {*}$args }

# @name[text][text]
# The name and brief description of the command.
proc block_name {name brief} {
  ::output "$name \\- [string trim $brief]\n"
  ::output ".br\n"
}

# @usage[inline]
# Renders the usage text used in a synopsis.
proc block_usage {text} {
  ::output "Usage: "
  ::fbld::inline inline_invoke [string trim $text]
  ::output "\n.br\n"
}

# @description[block]
proc block_description {text} {
  ::fbld::block block_invoke $text
}

# @par[inline]
# Default paragraph block.
proc block_par {text} {
  ::output ".sp\n"
  ::fbld::inline inline_invoke [string trim $text]
  ::output "\n.br\n"
}

# @defs[block]
# Used for a definition list.
proc block_defs {text} {
  ::fbld::block block_invoke $text
}

# @definition[name][value]
# Used for a long definition.
proc block_definition {name text} {
  ::output ".sp\n"
  ::fbld::inline inline_invoke [string trim $name]
  ::output ":\n.br\n.in +2\n"
  ::fbld::block block_invoke $text
  ::output "\n.in -2\n"
}

# @def[name][value]
# Used for a short definition.
proc block_def {name text} {
  ::fbld::inline inline_invoke [string trim $name]
  ::output "\n.br\n.in +4\n"
  ::fbld::inline inline_invoke [string trim $text]
  ::output "\n.br\n.in -4\n"
}

proc block_options {text} {
  ::fbld::block block_invoke $text
}

proc block_exitstatus {text} {
  ::output "Exit Status:\n.br\n.in +2\n"
  ::fbld::block block_invoke $text
  ::output "\n.in -2\n"
}

proc block_subsection {title body} {
  ::output "$title:\n.br\n.in +2\n"
  ::fbld::block block_invoke $body
  ::output "\n.in -2\n"
}

proc block_opt {opt desc} {
  block_def $opt $desc
}

proc block_examples {text} {
  ::output "Examples:\n.br\n.in +2\n"
  ::fbld::block block_invoke $text
  ::output "\n.in -2\n"
}

proc block_ex {text desc} {
  ::output ".sp\n"
  ::fbld::inline inline_invoke [string trim $text]
  ::output "\n.sp\n"
  ::fbld::block block_invoke $desc
}

proc block_ {text} {
  block_par $text
}

proc block_invoke {cmd args} {
  block_$cmd {*}$args
}

# cstrlit --
#   Create a c header file that defines a c string literal of the given name
#   with the given contents.
proc cstrlit { name text } {
  set output "const unsigned char $name\[\] = \{\n  ";
  set c 2
  set i 0
  set len [string length $text]
  while {$i < $len} {
    if {$c > 70} {
      append output "\n  "
      set c 2
    }
    set code "[scan [string index $text $i] %c], "
    append output $code
    incr c [string length $code]
    incr i
  }
  append output "\n\};"
  return $output
}

set input [lindex $argv 0]
set roff ""
proc ::output { line } {
  append ::roff "$line"
}

::output ".ll 78\n"
::fbld::block block_invoke [read [open $input "r"]]

set ascii [string trim [exec groff -T ascii << $roff]]
set header [cstrlit "fbldUsageHelpText" "$ascii\n"]
puts $header

