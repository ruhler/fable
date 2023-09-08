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
}
