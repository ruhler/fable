# fbld backend that checks the syntax of core tags.
#
# The defines a collection of fbld tags common to all backend targets.
# Tags are documented and checked for proper use.

# The following single argument procs are expected to be defined:
# ::unescape - Processes fbld escaped text.
# ::inline - Processes inline text.
# ::block - Processes block text.

# Handling of default inline text.
# Fbld escape characters are converted to normal characters.
# @param text  ESCAPED text to unescape.
proc inline_ {text} { ::unescape $text }

# @text[ESCAPED text]
# An explicit tag for default inline text.
# @param text  The text to unescape.
proc inline_text {text} { ::unescape $text }

# @l[ESCAPED text]
# A literal string.
# The text is highlighted as bold or monospace font, for example.
# @param text  The text to highlith.
proc inline_l {text} { ::unescape $text }

# @a[ESCAPED text]
# An argument.
# The text is highlighted using italics, for example.
# @param text  The text to highlight.
proc inline_a {text} { ::unescape $text }

# @label[ESCAPED id]
# Give a label to a point in the text.
# For use as a target of local or exteranl references.
# @param id  The id to use for the lable.
proc inline_label {text} { ::unescape $text }

# @ref[ESCAPED id][INLINE caption]
# Refer to a locally defined label.
# @param id  The id of a @label to link to.
# @param caption  Text to display.
proc inline_ref {id caption} {
  ::unescape $id
  ::inline $caption
}

# @url[ESCAPED url][? INLINE text]
# Reference a url.
# @param url  The target url.
# @param text  Optional text to display. Defaults to the url.
proc inline_url {url args} {
  switch -exact [llength $args] {
    0 { set text $url }
    1 { set text [lindex $args 0] }
    default { error "Too many arguments to @url" } 
  }
  ::unescape $url
  ::inline $text
}

# @file[ESCAPED file][? INLINE text]
# Reference a file on disk.
# @param file  The path to the file, relative to the source file.
# @param text  Optional text to display. Defaults to the file.
proc inline_file {file args} {
  switch -exact [llength $args] {
    0 { set text $file }
    1 { set text [lindex $args 0] }
    default { error "Too many arguments to @file" } 
  }
  ::unescape $file
  ::inline $text
}

# @fbld[ESCAPED file][? INLINE text]
# Reference to another fbld based document.
# @param file  The path to the fbld file, relative to the source file,
#   optionally followed by #<id>.
# @param text  Optional text to display. Defaults to the file.
proc inline_fbld {file args} {
  switch -exact [llength $args] {
    0 { set text $file }
    1 { set text [lindex $args 0] }
    default { error "Too many arguments to @fbld" } 
  }
  ::unescape $file
  ::inline $text
}

# Handling of default block text.
# The text is treated as a paragraph.
# @param text  INLINE text to perform inline processing on.
proc block_ {text} { ::inline $text }

# @par[INLINE text]
# A paragraph of text.
# @param text  The body of the paragraph.
proc block_par {text} { ::inline $text }

# @definition[INLINE name][BLOCK value]
# A long form definition of a term.
# @param name  The term to define.
# @param value  The definition of the term.
proc block_definition {name value} {
  ::inline $name
  ::block $value
}

# @def[INLINE name][INLINE value]
# A short form definition of a term.
# @param name  The term to define.
# @param value  The definition of the term.
proc block_def {name value} {
  ::inline $name
  ::inline $value
}

# @item[BLOCK text]
# A list item.
# @param text  The text of the item.
proc block_item {text} {
  ::block $text
}

# @code[ESCAPED language][ESCAPED text]
# A source code listing.
# @param language  The lanuage, e.g. c, python, sh, fble, vim, etc.
# @param text  The source code text.
proc block_code {language text} {
  ::unescape $language
  ::unescape $text
}

# @doc[INLINE title][BLOCK content]
# Top level entry point for a generic document.
# @param title  The title of the document.
# @param body  The contents of the document.
proc block_doc {title body} {
  ::inline $title
  ::block $body
}

# @section[INLINE title][BLOCK body]
# A top level section.
# @param title  The tile of the section.
# @param body  The contents of the section.
proc block_section {title body} {
  ::inline $title
  ::block $body
}

# @subsection[INLINE title][BLOCK body]
# A second level section.
# @param title  The tile of the section.
# @param body  The contents of the section.
proc block_subsection {title body} {
  ::inline $title
  ::block $body
}
