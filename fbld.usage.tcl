
# fbld processor for describing command usage.
source fbld.tcl

# Generate a man page.
# @title   Title to use for the man page.
# @input   The name of a .fbld file describing the usage.
proc man {title input} {
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

  # @name[text][text]
  # The name and brief description of the command.
  proc block_name {name brief} {
    puts {.SH "NAME"}
    puts "$name \\- $brief"
  }

  # @usage[inline]
  # Renders the usage text used in a synopsis.
  proc block_usage {text} {
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

  puts ".TH \"$title\" 1"
  ::fbld::block block_invoke [read [open $input "r"]]
}

# Generate roff formatted usage text.
# @arg input - The name of a .fbld file describing the usage.
proc roff {input} {
  # default command for line text.
  proc inline_ {text} {
    ::output [::fbld::unescape $text]
  }

  proc inline_a {text} { inline_ $text }
  proc inline_l {text} { inline_ $text }

  proc inline_invoke {cmd args} {
    inline_$cmd {*}$args
  }

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

  ::output ".ll 78\n"
  ::fbld::block block_invoke [read [open $input "r"]]
}

# cstrlit --
#   Create a c header file that defines a c string literal of the given name
#   with the given contents.
proc cstrlit { name text } {
  set output "const unsigned_char $name\[\] = \{\n  ";
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

#man [lindex $argv 0] [lindex $argv 1]

set roffstr ""
proc output { line } {
  append ::roffstr "$line"
}
roff [lindex $argv 0]

set ascii [string trim [exec groff -T ascii << $roffstr]]

set header [cstrlit "usage_text" $ascii]
puts $header

