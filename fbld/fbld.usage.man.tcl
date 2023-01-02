
# Converts fbld formatted, usage structured document to a man page.
# TODO: Use the generic runfbld.tcl infrastructure for this instead.
#
# Usage: tclsh8.6 fbld.usage.man.tcl title usage.fbld > usage.1
set fblddir [file dirname [file normalize [info script]]]
source $fblddir/fbld.tcl

# default command for line text.
proc inline_ {text} {
  puts -nonewline [::fbld::unescape $text]
}

# @a[text].
# Highlights text for an argument. In man pages we use italics for this.
proc inline_a {text} {
  puts -nonewline "\\fI$text\\fR"
}

# @l[text].
# Highlights text for a literal. In man pages we use bold for this.
proc inline_l {text} {
  puts -nonewline "\\fB$text\\fR"
}

proc inline_invoke {cmd args} {
  inline_$cmd {*}$args
}

# @usage[text][text][content]
# The name and brief description of the command.
proc block_usage {name brief content} {
  puts {.SH "NAME"}
  puts "$name \\- $brief"
  ::fbld::block block_invoke $content
}

# @synopsys[inline]
# Renders the usage text used in a synopsis.
proc block_synopsys {text} {
  puts {.SH "SYNOPSIS"}
  ::fbld::inline inline_invoke $text
}

# @description[block]
proc block_description {text} {
  puts {.SH "DESCRIPTION"}
  ::fbld::block block_invoke $text
}

# @par[inline]
# Default paragraph block.
proc block_par {text} {
  puts {.P}
  ::fbld::inline inline_invoke [string trim $text]
  puts ""
}

# @defs[block]
# Used for a definition list.
proc block_defs {text} {
  ::fbld::block block_invoke $text
}

# @definition[name][value]
# Used for a long definition.
proc block_definition {name text} {
  puts ".P"
  ::fbld::inline inline_invoke [string trim $name]
  puts "\n.RS"
  ::fbld::block block_invoke $text
  puts ".RE"
}

# @def[name][value]
# Used for a short definition.
proc block_def {name text} {
  puts ".TP 4"
  ::fbld::inline inline_invoke [string trim $name]
  puts ""
  ::fbld::inline inline_invoke [string trim $text]
  puts ""
}

proc block_options {text} {
  puts {.SH OPTIONS}
  ::fbld::block block_invoke $text
}

proc block_exitstatus {text} {
  puts {.SH EXIT STATUS}
  ::fbld::block block_invoke $text
}

proc block_subsection {title body} {
  puts ".SS $title"
  ::fbld::block block_invoke $body
}

proc block_opt {opt desc} {
  block_def $opt $desc
}

proc block_examples {text} {
  puts {.SH EXAMPLE}
  ::fbld::block block_invoke $text
}

proc block_ex {text desc} {
  puts ".P"
  puts -nonewline ".B "
  ::fbld::inline inline_invoke [string trim $text]
  puts "\n.P"
  ::fbld::block block_invoke $desc
}

proc block_ {text} {
  block_par $text
}

proc block_invoke {cmd args} {
  block_$cmd {*}$args
}

set title [lindex $argv 0]
set input [lindex $argv 1]

puts ".TH \"$title\" 1"
::fbld::block block_invoke [read [open $input "r"]]

