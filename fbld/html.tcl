# Html backend for fbld.
# Converts core fbld document tags to html.

# The following global variables are expected to be defined:
#   ::unescape - A single argument proc that removes fbld escape sequences.
#   ::inline - A single argument proc that processes inline text.
#   ::block - A single argument proc that processes block text.
#   ::output - A single argument proc to output a string.

# Handling of default inline text.
# Fbld escape characters are converted to normal characters.
# @param text  ESCAPED text to unescape.
proc inline_ {text} {
  ::output [string map { & &amp; < &lt; > &gt; } [::unescape $text]]
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
  ::output "<code>"
  inline_text $text
  ::output "</code>"
}

# @a[ESCAPED text]
# An argument.
# The text is highlighted using italics, for example.
# @param text  The text to highlight.
proc inline_a {text} {
  ::output "<i>"
  inline_text $text
  ::output "</i>"
}

# @label[ESCAPED id][INLINE text]
# Give a label to a point in the text.
# For use as a target of local or external references.
# @param id  The id to use for the lable.
# @param text  The text to display for the label target.
proc inline_label {id text} {
  ::output "<a id=\""
  inline_text $id
  ::output "\">"
  ::inline $text
  ::output "</a>"
}

# @ref[ESCAPED id][INLINE caption]
# Refer to a locally defined label.
# @param id  The id of a @label to link to.
# @param caption  Text to display.
proc inline_ref {id caption} {
  ::output "<a href=\"#"
  inline_text $id
  ::output "\">"
  ::inline $caption
  ::output "</a>"
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

  ::output "<a href=\"$url\">"
  ::inline $text
  ::output "</a>"
}

# @file[ESCAPED file][? INLINE text]
# Reference a file on disk.
# @param file  The path to the file.
# @param text  Optional text to display. Defaults to the file.
proc inline_file {file args} {
  switch -exact [llength $args] {
    0 { set text $file }
    1 { set text [lindex $args 0] }
    default { error "Too many arguments to @file" } 
  }
  ::output "<a href=\"$file\"><code>"
  ::inline $text
  ::output "</code></a>"
}

# @fbld[ESCAPED file][? INLINE text]
# Reference to another fbld based document.
# @param file  The path to the fbld file, optionally followed by #<id>.
# @param text  Optional text to display. Defaults to the file.
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
  ::inline $text
  ::output "</code></a>"
}

# Handling of default block text.
# The text is treated as a paragraph.
# @param text  INLINE text to perform inline processing on.
proc block_ {text} {
  ::output "<p>"
  ::inline $text
  ::output "</p>\n"
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
  ::output "<dl>\n  <dt>"
  ::inline $name
  ::output "</dt>\n  <dd>"
  ::block $value
  ::output "</dd>\n</dl>\n"
}

# @def[INLINE name][INLINE value]
# A short form definition of a term.
# @param name  The term to define.
# @param value  The definition of the term.
proc block_def {name value} {
  ::output "<dl>\n  <dt>"
  ::inline $name
  ::output "</dt>\n  <dd>"
  ::inline $value
  ::output "</dd>\n</dl>\n"
}

# @item[BLOCK text]
# A list item.
# @param text  The text of the item.
proc block_item {text} {
  ::output "<ul><li>"
  ::block $text
  ::output "</li></ul>\n"
}

# @code[ESCAPED language][ESCAPED text]
# A source code listing.
# @param language  The lanuage, e.g. c, python, sh, fble, vim, etc.
# @param text  The source code text.
proc block_code {language text} {
  # TODO: Have a better way to specify what should be used for 'language'.
  set langopt ""
  set fblddir [file dirname [file normalize [info script]]]
  switch -exact $language {
    fbld { set langopt "--src-lang fbld --lang-def $fblddir/fbld.lang" }
    fble { set langopt "--src-lang fble --lang-def $fblddir/../spec/fble.lang" }
    vim { set langopt "--src-lang txt" }
    default { set langopt "--src-lang $language" }
  }

  set formatted [exec source-highlight {*}$langopt -o STDOUT << [::unescape $text]]

  ::output $formatted
}

# @doc[INLINE title][BLOCK body]
# Top level entry point for a generic document.
# @param title  The title of the document.
# @param body  The contents of the document.
proc block_doc {title body} {
  ::output "<head>\n"
  ::output {
<style>
body {
  color: #333333;
  font-family: sans-serif;
  max-width: 50em;
}
h1 { font-size: 2.0em; }
h2 { font-size: 1.6em; }
h3 { font-size: 1.4em; }
h4 { font-size: 1.2em; }
p { font-size: 1em; }
</style>
}
  ::output "<title>$title</title>\n"
  ::output "</head><body>"

  ::output "<h1>"
  ::inline $title
  ::output "</h1>\n"
  ::block $body
  ::output "</body>"
}

# @section[INLINE title][BLOCK body]
# A top level section.
# @param title  The tile of the section.
# @param body  The contents of the section.
proc block_section {title body} {
  ::output "<h2>"
  ::inline $title
  ::output "</h2>\n"
  ::block $body
}

# @subsection[INLINE title][BLOCK body]
# A second level section.
# @param title  The tile of the section.
# @param body  The contents of the section.
proc block_subsection {title body} {
  ::output "<h3>"
  ::inline $title
  ::output "</h3>\n"
  ::block $body
}

# @subsubsection[INLINE title][BLOCK body]
# A third level section.
# @param title  The tile of the section.
# @param body  The contents of the section.
proc block_subsubsection {title body} {
  ::output "<h4>"
  ::inline $title
  ::output "</h3>\n"
  ::block $body
}
