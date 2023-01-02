# Html backend for fbld.

# The following global variables are expected to be defined:
#   ::invoke_inline - A single argument proc that processes inline text.
#   ::invoke_block - A single argument proc that processes block text.
#   ::output - A single argument proc to output a string.
# And everything else defined in fbld.tcl.
namespace eval fbld/html {
  # Handling of default inline text.
  # Fbld escape characters are converted to normal characters.
  # @param text  ESCAPED text to unescape.
  namespace export inline_
  proc inline_ {text} {
    # TODO: Do escapes needed for html.
    ::output $text
  }

  # @text[ESCAPED text]
  # An explicit tag for default inline text.
  # @param text  The text to unescape.
  namespace export inline_text
  proc inline_text {text} {
    inline_ $text
  }

  # @l[ESCAPED text]
  # A literal string.
  # The text is highlighted as bold or monospace font, for example.
  # @param text  The text to highlith.
  namespace export inline_l
  proc inline_l {text} {
    ::output "<code>"
    inline_text $text
    ::output "</code>"
  }

  # @a[ESCAPED text]
  # An argument.
  # The text is highlighted using italics, for example.
  # @param text  The text to highlight.
  namespace export inline_a
  proc inline_a {text} {
    ::output "<i>"
    inline_text $text
    ::output "</i>"
  }

  # @label[ESCAPED id]
  # Give a label to a point in the text.
  # For use as a target of local or external references.
  # @param id  The id to use for the lable.
  namespace export inline_label
  proc inline_label {text} {
    ::output "<a id=\""
    inline_text $text
    ::output "\"></a>"
  }

  # @ref[ESCAPED id][INLINE caption]
  # Refer to a locally defined label.
  # @param id  The id of a @label to link to.
  # @param caption  Text to display.
  namespace export inline_ref
  proc inline_ref {id caption} {
    ::output "<a href=\"#"
    inline_text $id
    ::output "\">"
    ::fbld::inline ::invoke_inline $caption
    ::output "</a>"
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

    ::output "<a href=\"$url\">"
    ::fbld::inline ::invoke_inline $text
    ::output "</a>"
  }

  # @file[ESCAPED file][? INLINE text]
  # Reference a file on disk.
  # @param file  The path to the file.
  # @param text  Optional text to display. Defaults to the file.
  namespace export inline_file
  proc inline_file {file args} {
    switch -exact [llength $args] {
      0 { set text $file }
      1 { set text [lindex $args 0] }
      default { error "Too many arguments to @file" } 
    }
    ::output "<a href=\"$file\"><code>"
    ::fbld::inline ::invoke_inline $text
    ::output "</code></a>"
  }

  # @fbld[ESCAPED file][? INLINE text]
  # Reference to another fbld based document.
  # @param file  The path to the fbld file, optionally followed by #<id>.
  # @param text  Optional text to display. Defaults to the file.
  namespace export inline_fbld
  proc inline_fbld {file args} {
    switch -exact [llength $args] {
      0 { set text $file }
      1 { set text [lindex $args 0] }
      default { error "Too many arguments to @fbld" } 
    }

    # TODO: Is it okay to assume the target is also translated using the html
    # backend?
    set target [string map {.fbld .html} $file] 
    ::output "<a href=\"$target\"><code>"
    ::fbld::inline ::invoke_inline $text
    ::output "</code></a>"
  }

  # Handling of default block text.
  # The text is treated as a paragraph.
  # @param text  INLINE text to perform inline processing on.
  namespace export block_
  proc block_ {text} {
    ::output "<p>"
    ::fbld::inline ::invoke_inline $text
    ::output "</p>\n"
  }

  # @par[INLINE text]
  # A paragraph of text.
  # @param text  The body of the paragraph.
  namespace export block_par
  proc block_par {text} {
    block_ $text
  }

  # @definition[INLINE name][BLOCK value]
  # A long form definition of a term.
  # @param name  The term to define.
  # @param value  The definition of the term.
  namespace export block_definition
  proc block_definition {name value} {
    ::output "<dl>\n  <dt>"
    ::fbld::inline ::invoke_inline $name
    ::output "</dt>\n  <dd>"
    ::fbld::block ::invoke_block $value
    ::output "</dd>\n</dl>\n"
  }

  # @def[INLINE name][INLINE value]
  # A short form definition of a term.
  # @param name  The term to define.
  # @param value  The definition of the term.
  namespace export block_def
  proc block_def {name value} {
    ::output "<dl>\n  <dt>"
    ::fbld::inline ::invoke_inline $name
    ::output "</dt>\n  <dd>"
    ::fbld::block ::invoke_block $value
    ::output "</dd>\n</dl>\n"
  }

  # @code[ESCAPED language][ESCAPED text]
  # A source code listing.
  # @param language  The lanuage, e.g. c, python, sh, fble, vim, etc.
  # @param text  The source code text.
  namespace export block_code
  proc block_code {language text} {
    ::output "<pre>"
    inline_text $text
    ::output "</pre>"
  }

  # @section[INLINE title][BLOCK body]
  # A top level section.
  # @param title  The tile of the section.
  # @param body  The contents of the section.
  namespace export block_section
  proc block_section {title body} {
    ::output "<h1>"
    ::fbld::inline ::invoke_inline $title
    ::output "</h1>\n"
    ::fbld::block ::invoke_block $body
  }

  # @subsection[INLINE title][BLOCK body]
  # A second level section.
  # @param title  The tile of the section.
  # @param body  The contents of the section.
  namespace export block_subsection
  proc block_subsection {title body} {
    ::output "<h2>"
    ::fbld::inline ::invoke_inline $title
    ::output "</h2>\n"
    ::fbld::block ::invoke_block $body
  }
}
