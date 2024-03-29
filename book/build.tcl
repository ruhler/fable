namespace eval "book" {
  set docs {
    Book.fbld
    Origins.fbld
    Performance.fbld
    Profiler.fbld
    Syntax.fbld
    TypeSystem.fbld
  }

  foreach {x} $docs {
    set base [file rootname [file tail $x]]
    ::fbld_html_doc $::b/www/book/$base.html $::s/book/$base.fbld
    www book/$base.html
  }

  build $::b/www/book/index.html $::b/www/book/Book.html \
    "cp $::b/www/book/Book.html $::b/www/book/index.html"
  www book/index.html
}
