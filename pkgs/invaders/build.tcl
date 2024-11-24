namespace eval "pkgs/invaders" {
  fbld_help_fble_usage $::b/pkgs/invaders/Invaders/Usage.fble $::s/pkgs/invaders/fble-invaders.fbld

  pkg invaders [list core app] $::b/pkgs/invaders/Invaders/Usage.fble ""

  # /Invaders/Tests% interpreted
  set cflags "-I $::s/pkgs/invaders -I $::b/pkgs/invaders -I $::s/pkgs/app -I $::s/pkgs/core"
  testsuite $::b/pkgs/invaders/tests-interpreted.tr "$::b/pkgs/core/fble-stdio $::b/pkgs/invaders/Invaders/Tests.fble.d" \
    "$::b/pkgs/core/fble-stdio $cflags -m /Invaders/Tests%"

  # /Invaders/Tests% compiled
  stdio $::b/pkgs/invaders/invaders-tests "/Invaders/Tests%" "app invaders"
  testsuite $::b/pkgs/invaders/tests-compiled.tr $::b/pkgs/invaders/invaders-tests \
    "$::b/pkgs/invaders/invaders-tests"

  if $::config::enable_fble_app {
    fbld_man_usage $::b/pkgs/invaders/fble-invaders.1 $::s/pkgs/invaders/fble-invaders.fbld
    install $::b/pkgs/invaders/fble-invaders.1 $::config::mandir/man1/fble-invaders.1
    app $::b/pkgs/invaders/fble-invaders "/Invaders/AppIO%" "invaders"
    install $::b/pkgs/invaders/fble-invaders $::config::bindir/fble-invaders

    test $::b/pkgs/invaders/fble-invaders.tr \
      "$::b/pkgs/invaders/fble-invaders" \
      "$::b/pkgs/invaders/fble-invaders --driver dummy --profile $::b/pkgs/invaders/fble-invaders.prof -- --test"
  }
}
