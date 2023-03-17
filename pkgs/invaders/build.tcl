namespace eval "pkgs/invaders" {
  dist_s $::s/pkgs/invaders/build.tcl
  dist_s $::s/pkgs/invaders/invaders.fbld
  dist_s $::s/pkgs/invaders/Invaders/speeds.txt

  pkg invaders [list core app] ""

  # /Invaders/Tests% interpreted
  set cflags "-I $::s/pkgs/invaders -I $::s/pkgs/app -I $::s/pkgs/core"
  test $::b/pkgs/invaders/tests-interpreted.tr "$::b/pkgs/core/fble-stdio $::b/pkgs/invaders/Invaders/Tests.fble.d" \
    "$::b/pkgs/core/fble-stdio $cflags -m /Invaders/Tests%"

  # /Invaders/Tests% compiled
  stdio $::b/pkgs/invaders/invaders-tests "/Invaders/Tests%" "invaders app"
  test $::b/pkgs/invaders/tests-compiled.tr $::b/pkgs/invaders/invaders-tests \
    "$::b/pkgs/invaders/invaders-tests"

  if $::config::enable_fble_app {
    man_usage $::b/pkgs/invaders/fble-invaders.1 $::s/pkgs/invaders/invaders.fbld
    install $::b/pkgs/invaders/fble-invaders.1 $::config::mandir/man1/fble-invaders.1
    app $::b/pkgs/invaders/fble-invaders "/Invaders/AppIO%" "invaders"
    install $::b/pkgs/invaders/fble-invaders $::config::bindir/fble-invaders
  }
}
