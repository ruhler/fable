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
    "$::b/pkgs/fbld2/fbld2 $::s/fbld2/nobuildstamp.fbld $::b/fbld2/version.fbld $::s/fbld2/markdown.fbld $::s/README.2.fbld" \
    "$::b/pkgs/fbld2/fbld2 $::s/fbld2/nobuildstamp.fbld $::b/fbld2/version.fbld $::s/fbld2/markdown.fbld $::s/README.2.fbld > $::b/pkgs/fbld2/README.md"

  # Make sure the version of README.md checked in matches the latest version.
  test $::b/pkgs/fbld2/README.tr \
    "$::s/README.md $::b/pkgs/fbld2/README.md" \
    "cmp $::s/README.md $::b/pkgs/fbld2/README.md"

  # README.html
  build $::b/pkgs/fbld2/README.html \
    "$::b/pkgs/fbld2/fbld2 $::s/buildstamp $::b/fbld2/version.fbld $::s/fbld2/html.fbld $::s/README.2.fbld" \
    "$::s/buildstamp --fbld2 BuildStamp | $::b/pkgs/fbld2/fbld2 - $::b/fbld2/version.fbld $::s/fbld2/html.fbld $::s/README.2.fbld > $::b/pkgs/fbld2/README.html"
}
