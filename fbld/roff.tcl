# roff backend for fbld.
# Converts core fbld document to roff.

# The following global variables are expected to be defined:
#   ::unescape - A single argument proc that removes fbld escape sequences.
#   ::inline - A single argument proc that processes inline text.
#   ::block - A single argument proc that processes block text.
#   ::output - A single argument proc to output a string.

# Handling of default inline text.
# Fbld escape characters are converted to normal characters.
# @param text  ESCAPED text to unescape.
proc inline_ {text} {
  ::output [::unescape $text]
}

# @text[ESCAPED text]
# An explicit tag for default inline text.
# @param text  The text to unescape.
proc inline_text {text} {
  inline_ $text
}

# @l[ESCAPED text]
# A literal string.
# The text is highlighted as bold or monospace font, for example.
# @param text  The text to highlith.
proc inline_l {text} {
  ::output "\\fB"
  inline_text $text
  ::output "\\fR"
}

# @a[ESCAPED text]
# An argument.
# The text is highlighted using italics, for example.
# @param text  The text to highlight.
proc inline_a {text} {
  ::output "\\fI"
  inline_text $text
  ::output "\\fR"
}

# @label[ESCAPED id]
# Give a label to a point in the text.
# For use as a target of local or external references.
# @param id  The id to use for the lable.
proc inline_label {text} {
  # TODO: Should we do something with the label?
}

# @ref[ESCAPED id][INLINE caption]
# Refer to a locally defined label.
# @param id  The id of a @label to link to.
# @param caption  Text to display.
proc inline_ref {id caption} {
  # TODO: Should we link somehow to the label?
  ::inline $caption
}

# @url[ESCAPED url][? INLINE text]
# Reference a url.
# @param url  The target url.
# @param text  Optional text to display. Defaults to the url.
proc inline_url {url args} {
  # TODO: Can we do something better here?
  switch -exact [llength $args] {
    0 {
      ::inline $url
    }
    1 {
      ::inline "[lindex 0 $args]($url)"
    }
    default { error "Too many arguments to @url" } 
  }
}

# @file[ESCAPED file][? INLINE text]
# Reference a file on disk.
# @param file  The path to the file.
# @param text  Optional text to display. Defaults to the file.
proc inline_file {file args} {
  # TODO: Can we do something better here?
  switch -exact [llength $args] {
    0 {
      ::inline $file
    }
    1 {
      ::inline "[lindex 0 $args]($file)"
    }
    default { error "Too many arguments to @file" } 
  }
}

# @fbld[ESCAPED file][? INLINE text]
# Reference to another fbld based document.
# @param file  The path to the fbld file, optionally followed by #<id>.
# @param text  Optional text to display. Defaults to the file.
proc inline_fbld {file args} {
  # TODO: Can we do something better here?
  switch -exact [llength $args] {
    0 {
      ::inline $file
    }
    1 {
      ::inline "[lindex 0 $args]($file)"
    }
    default { error "Too many arguments to @fbld" } 
  }
}

# Handling of default block text.
# The text is treated as a paragraph.
# @param text  INLINE text to perform inline processing on.
proc block_ {text} {
  ::output ".sp\n"
  ::inline [string trim $text]
  ::output "\n.br\n"
}

# @par[INLINE text]
# A paragraph of text.
# @param text  The body of the paragraph.
proc block_par {text} {
  block_ $text
}

# @definition[INLINE name][BLOCK value]
# A long form definition of a term.
# @param name  The term to define.
# @param value  The definition of the term.
proc block_definition {name value} {
  ::output ".sp\n"
  ::inline [string trim $name]
  ::output ":\n.br\n.in +2\n"
  ::block $value
  ::output "\n.in -2\n"
}

# @def[INLINE name][INLINE value]
# A short form definition of a term.
# @param name  The term to define.
# @param value  The definition of the term.
proc block_def {name value} {
  ::inline [string trim $name]
  ::output "\n.br\n.in +4\n"
  ::inline [string trim $value]
  ::output "\n.br\n.in -4\n"
}

# @item[BLOCK text]
# A list item.
# @param text  The text of the item.
proc block_item {text} {
  # TODO: Do something better here.
  ::output "* "
  ::block $text
}

# @code[ESCAPED language][ESCAPED text]
# A source code listing.
# @param language  The lanuage, e.g. c, python, sh, fble, vim, etc.
# @param text  The source code text.
proc block_code {language text} {
  # TODO: Do something better here?
  inline_text $text
}

# @doc[INLINE title][BLOCK body]
# Top level entry point for a generic document.
# @param title  The title of the document.
# @param body  The contents of the document.
proc block_doc {title body} {
  # TODO: This probably isn't the place to enforce these output parameters.
  # Come up with some better way?
  ::output ".pl 1\n"
  ::output ".ll 78\n"
  ::block $body
}

# @section[INLINE title][BLOCK body]
# A top level section.
# @param title  The tile of the section.
# @param body  The contents of the section.
proc block_section {title body} {
  # TODO: Come up with a better approach for sections
  ::output "$title:\n.br\n.in +2\n"
  ::block $body
  ::output "\n.in -2\n"
}

# @subsection[INLINE title][BLOCK body]
# A second level section.
# @param title  The tile of the section.
# @param body  The contents of the section.
proc block_subsection {title body} {
  # TODO: Come up with a better approach for subsections
  ::output "$title:\n.br\n.in +2\n"
  ::block $body
  ::output "\n.in -2\n"
}

# @subsubsection[INLINE title][BLOCK body]
# A third level section.
# @param title  The tile of the section.
# @param body  The contents of the section.
proc block_subsubsection {title body} {
  # TODO: Come up with a better approach for subsubsections
  ::output "$title:\n.br\n.in +2\n"
  block_definition $title $body
  ::output "\n.in -2\n"
}
