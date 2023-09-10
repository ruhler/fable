namespace eval "pkgs/fbld2" {
  pkg fbld2 [list core] ""

  # Fbld/Tests compiled
  stdio $::b/pkgs/fbld2/fbld-tests "/Fbld/Tests%" "fbld2"
  testsuite $::b/pkgs/fbld2/fbld-tests.tr $::b/pkgs/fbld2/fbld-tests \
    "$::b/pkgs/fbld2/fbld-tests"

  # fbld2
  stdio $::b/pkgs/fbld2/fbld2 "/Fbld/Main%" "fbld2"
  install $::b/pkgs/fbld2/fbld2 $::config::bindir/fbld2

  # version.fbld
  build $::b/fbld2/version.fbld "" \
    "echo @define\[FbleVersion\]\[\]\[$::version\] @@ > $::b/fbld2/version.fbld"

  # README.md
  build $::b/pkgs/fbld2/README.md \
    "$::b/pkgs/fbld2/fbld2 $::s/fbld2/nobuildstamp.fbld $::b/fbld2/version.fbld $::s/fbld2/markdown.fbld $::s/README.fbld" \
    "$::b/pkgs/fbld2/fbld2 $::s/fbld2/nobuildstamp.fbld $::b/fbld2/version.fbld $::s/fbld2/markdown.fbld $::s/README.fbld > $::b/pkgs/fbld2/README.md"

  # Make sure the version of README.md checked in matches the latest version.
  test $::b/pkgs/fbld2/README.tr \
    "$::s/README.md $::b/pkgs/fbld2/README.md" \
    "cmp $::s/README.md $::b/pkgs/fbld2/README.md"

  proc ::fbld2_html_doc { target sources } {
    build $target \
      "$::b/pkgs/fbld2/fbld2 $::s/buildstamp $::b/fbld2/version.fbld $::s/fbld2/html.fbld $sources" \
      "$::s/buildstamp --fbld2 BuildStamp | $::b/pkgs/fbld2/fbld2 - $::b/fbld2/version.fbld $::s/fbld2/html.fbld $sources > $target"
  }

  proc ::fbld2_html_tutorial { target source } {
    fbld2_html_doc $target [list $::s/tutorials/tutorial.lib.fbld $source]
  }

  #fbld2_html_doc $::b/pkgs/fbld2/fble.html "$::s/spec/fble.lib.fbld $::s/spec/fble.fbld"
  #fbld2_html_doc $::b/pkgs/fbld2/HelloWorld.html "$::s/tutorials/tutorial.lib.fbld $::s/tutorials/HelloWorld.fbld"

  #www $::b/pkgs/fbld2/fble.html
  #www $::b/pkgs/fbld2/HelloWorld.html

  # Fbld Spec www.
  # TODO: Switch to fbld2 spec once that is available.
  ::fbld2_html_doc $::b/www/fbld/fbld.html $::s/fbld/fbld.fbld
  www $::b/www/fbld/fbld.html
}
