# Core fbld tags.
#
# The defines a collection of fbld tags common to all backend targets.
# Tags are documented and checked for proper use.

# The following global variables are expected to be defined:
#   ::invoke_inline - A single argument proc that processes inline text.
#   ::invoke_block - A single argument proc that processes block text.
# And everything else defined in fbld.tcl.
namespace eval fbld/core {
  # Types of arguments:
  #   ESCAPED - Unadorned text that may contain fbld escape characters.
  #   INLINE - Text that may contain fbld inline tags.
  #   BLOCK - Text that may contain fbld block tags.
  proc check_escaped { text } { ::fbld::unescape $text }
  proc check_inline { text } { ::fbld::inline ::invoke_inline $text }
  proc check_block { text } { ::fbld::block ::invoke_block $text }

  # Handling of default inline text.
  # Fbld escape characters are converted to normal characters.
  # @param text  ESCAPED text to unescape.
  namespace export inline_
  proc inline_ {text} { check_escaped $text }

  # @text[ESCAPED text]
  # An explicit tag for default inline text.
  # @param text  The text to unescape.
  namespace export inline_text
  proc inline_text {text} { check_escaped $text }

  # @l[ESCAPED text]
  # A literal string.
  # The text is highlighted as bold or monospace font, for example.
  # @param text  The text to highlith.
  namespace export inline_l
  proc inline_l {text} { check_escaped $text }

  # @a[ESCAPED text]
  # An argument.
  # The text is highlighted using italics, for example.
  # @param text  The text to highlight.
  namespace export inline_a
  proc inline_a {text} { check_escaped $text }

  # @label[ESCAPED id]
  # Give a label to a point in the text.
  # For use as a target of local or exteranl references.
  # @param id  The id to use for the lable.
  namespace export inline_label
  proc inline_label {text} { check_escaped $text }

  # @ref[ESCAPED id][INLINE caption]
  # Refer to a locally defined label.
  # @param id  The id of a @label to link to.
  # @param caption  Text to display.
  namespace export inline_ref
  proc inline_ref {id caption} {
    check_escaped $id
    check_inline $caption
  }

  # @url[ESCAPED url][? INLINE text]
  # Reference a url.
  # @param url  The target url.
  # @param text  Optional text to display. Defaults to the url.
  namespace export inline_url
  proc inline_url {url args} {
    switch -exact [llength $args] {
      0 { set text $url }
      1 { set text [lindex $args 0] }
      default { error "Too many arguments to @url" } 
    }
    check_escaped $url
    check_inline $text
  }

  # @file[ESCAPED file][? INLINE text]
  # Reference a file on disk.
  # @param file  The path to the file, relative to the source file.
  # @param text  Optional text to display. Defaults to the file.
  namespace export inline_file
  proc inline_file {file args} {
    switch -exact [llength $args] {
      0 { set text $file }
      1 { set text [lindex $args 0] }
      default { error "Too many arguments to @file" } 
    }
    check_escaped $file
    check_inline $text
  }

  # @fbld[ESCAPED file][? INLINE text]
  # Reference to another fbld based document.
  # @param file  The path to the fbld file, relative to the source file,
  #   optionally followed by #<id>.
  # @param text  Optional text to display. Defaults to the file.
  namespace export inline_fbld
  proc inline_fbld {file args} {
    switch -exact [llength $args] {
      0 { set text $file }
      1 { set text [lindex $args 0] }
      default { error "Too many arguments to @fbld" } 
    }
    check_escaped $file
    check_inline $text
  }

  # Handling of default block text.
  # The text is treated as a paragraph.
  # @param text  INLINE text to perform inline processing on.
  namespace export block_
  proc block_ {text} { check_inline $text }

  # @par[INLINE text]
  # A paragraph of text.
  # @param text  The body of the paragraph.
  namespace export block_par
  proc block_par {text} { check_inline $text }

  # @definition[INLINE name][BLOCK value]
  # A long form definition of a term.
  # @param name  The term to define.
  # @param value  The definition of the term.
  namespace export block_definition
  proc block_definition {name value} {
    check_inline $name
    check_block $value
  }

  # @def[INLINE name][INLINE value]
  # A short form definition of a term.
  # @param name  The term to define.
  # @param value  The definition of the term.
  namespace export block_def
  proc block_def {name value} {
    check_inline $name
    check_inline $value
  }

  # @code[ESCAPED language][ESCAPED text]
  # A source code listing.
  # @param language  The lanuage, e.g. c, python, sh, fble, vim, etc.
  # @param text  The source code text.
  namespace export block_code
  proc block_code {language text} {
    check_escaped $language
    check_escaped $text
  }

  # @section[INLINE title][BLOCK body]
  # A top level section.
  # @param title  The tile of the section.
  # @param body  The contents of the section.
  namespace export block_section
  proc block_section {title body} {
    check_inline $title
    check_block $body
  }

  # @subsection[INLINE title][BLOCK body]
  # A second level section.
  # @param title  The tile of the section.
  # @param body  The contents of the section.
  namespace export block_subsection
  proc block_subsection {title body} {
    check_inline $title
    check_block $body
  }
}
