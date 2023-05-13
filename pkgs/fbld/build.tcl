namespace eval "pkgs/fbld" {
  pkg fbld [list core] ""

  # Fbld/Tests interpreted
  testsuite $::b/pkgs/fbld/Fbld/tests.tr "$::b/pkgs/core/fble-stdio $::b/pkgs/fbld/Fbld/Tests.fble.d" \
    "$::b/pkgs/core/fble-stdio -I $::s/pkgs/core -I $::s/pkgs/fbld -m /Fbld/Tests% --prefix Interpreted."

  # Fbld/Tests compiled
  stdio $::b/pkgs/fbld/fbld-tests "/Fbld/Tests%" "fbld"
  testsuite $::b/pkgs/fbld/fbld-tests.tr $::b/pkgs/fbld/fbld-tests \
    "$::b/pkgs/fbld/fbld-tests --prefix Compiled."

  # fbld-html-doc
  stdio $::b/pkgs/fbld/fbld-html-doc "/Fbld/Main/HtmlDoc%" "fbld"
  install $::b/pkgs/fbld/fbld-html-doc $::config::bindir/fbld-html-doc

  # Fbld Spec www.
  build $::b/pkgs/fbld/fbld.html \
    "$::b/pkgs/fbld/fbld-html-doc $::s/fbld/fbld.fbld" \
    "$::b/pkgs/fbld/fbld-html-doc $::s/fbld/fbld.fbld > $::b/pkgs/fbld/fbld.html"
}
