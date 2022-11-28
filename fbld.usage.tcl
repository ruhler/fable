
# fbld processor for describing command usage.
source fbld.tcl

# Generate a man page.
# @arg input - The name of a .fbld file describing the usage.
# @output output - The name of the output man page file.
proc man {title input} {
  # default command for line text.
  proc inline_ {text} {
    puts -nonewline $text
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
    puts {.SY}
    ::fbld::inline inline_invoke $text
    puts {.YS}
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
    ::fbld::inline inline_invoke $text
  }

  # @defs[block]
  # Used for a definition list.
  proc block_defs {text} {
    ::fbld::block block_invoke $text
  }

  # @def[name][value]
  # Used for a definition.
  proc block_def {name text} {
    puts {.TP}
    ::fbld::inline inline_invoke $name
    ::fbld::inline inline_invoke $text
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
    puts ".TP"
    ::fbld::inline inline_invoke $opt
    ::fbld::block block_invoke $desc
  }

  proc block_examples {text} {
    puts {.SH EXAMPLE}
    ::fbld::block block_invoke $text
  }

  proc block_ex {text desc} {
    puts {.EX}
    ::fbld::inline inline_invoke $text
    puts {.EE}
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

man [lindex $argv 0] [lindex $argv 1]
