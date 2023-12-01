namespace eval "tutorials" {
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
    www $::b/www/book/$base.html
  }
}
