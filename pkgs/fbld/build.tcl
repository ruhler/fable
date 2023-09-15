namespace eval "pkgs/fbld" {
  pkg fbld [list core] ""

  # Fbld/Tests compiled
  stdio $::b/pkgs/fbld/fbld-tests "/Fbld/Tests%" "fbld"
  testsuite $::b/pkgs/fbld/fbld-tests.tr $::b/pkgs/fbld/fbld-tests \
    "$::b/pkgs/fbld/fbld-tests"

  # fbld
  stdio $::b/pkgs/fbld/fbld "/Fbld/Main%" "fbld"
  install $::b/pkgs/fbld/fbld $::config::bindir/fbld

  # version.fbld
  build $::b/fbld/version.fbld "" \
    "echo @define\[FbleVersion\]\[\]\[$::version\] @@ > $::b/fbld/version.fbld"

  # README.md
  build $::b/pkgs/fbld/README.md \
    "$::b/pkgs/fbld/fbld $::s/fbld/nobuildstamp.fbld $::b/fbld/version.fbld $::s/fbld/markdown.fbld $::s/README.fbld" \
    "$::b/pkgs/fbld/fbld $::s/fbld/nobuildstamp.fbld $::b/fbld/version.fbld $::s/fbld/markdown.fbld $::s/README.fbld > $::b/pkgs/fbld/README.md"

  # Make sure the version of README.md checked in matches the latest version.
  test $::b/pkgs/fbld/README.tr \
    "$::s/README.md $::b/pkgs/fbld/README.md" \
    "cmp $::s/README.md $::b/pkgs/fbld/README.md"

  proc ::fbld_html_doc { target sources } {
    build $target \
      "$::b/pkgs/fbld/fbld $::s/buildstamp $::b/fbld/version.fbld $::s/fbld/html.fbld $sources" \
      "$::s/buildstamp --fbld BuildStamp | $::b/pkgs/fbld/fbld - $::b/fbld/version.fbld $::s/fbld/html.fbld $sources > $target"
  }

  proc ::fbld_html_tutorial { target source } {
    fbld_html_doc $target [list $::s/tutorials/tutorial.lib.fbld $source]
  }

  # Fbld Spec www.
  # TODO: Switch to fbld2 spec once that is available.
  ::fbld_html_doc $::b/www/fbld/fbld.html $::s/fbld/fbld.fbld
  www $::b/www/fbld/fbld.html

  # Fble Spec www.
  fbld_html_doc $::b/www/spec/fble.html "$::s/spec/fble.lib.fbld $::s/spec/fble.fbld"
  www $::b/www/spec/fble.html
}
