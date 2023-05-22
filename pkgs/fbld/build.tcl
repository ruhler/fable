namespace eval "pkgs/fbld" {
  # /Fbld/Fble/Version/Value%
  build $::b/pkgs/fbld/Fbld/Fble/Version/Value.fble "" \
    "echo \"/Core/String%.Str|'$::version';\" > $::b/pkgs/fbld/Fbld/Fble/Version/Value.fble"

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

  # fbld-markdown-doc
  stdio $::b/pkgs/fbld/fbld-markdown-doc "/Fbld/Main/MarkdownDoc%" "fbld"
  install $::b/pkgs/fbld/fbld-markdown-doc $::config::bindir/fbld-markdown-doc

  proc fbld_html_doc { target source } {
    build $target \
      "$::b/pkgs/fbld/fbld-html-doc $source $::s/buildstamp" \
      "$::b/pkgs/fbld/fbld-html-doc $source --buildstamp `$::s/buildstamp` > $target"
  }

  fbld_html_doc $::b/pkgs/fbld/fbld.html $::s/fbld/fbld.fbld
  fbld_html_doc $::b/pkgs/fbld/fble.html $::s/spec/fble.fbld
  fbld_html_doc $::b/pkgs/fbld/README.html $::s/README.fbld

  # README.md
  build $::b/pkgs/fbld/README.md \
    "$::b/pkgs/fbld/fbld-markdown-doc $::s/README.fbld" \
    "$::b/pkgs/fbld/fbld-markdown-doc --buildstamp '' $::s/README.fbld > $::b/pkgs/fbld/README.md"

  # Make sure the version of README.md checked in matches the latest version.
  test $::b/pkgs/fbld/README.tr \
    "$::s/README.md $::b/pkgs/fbld/README.md" \
    "cmp $::s/README.md $::b/pkgs/fbld/README.md"
}
