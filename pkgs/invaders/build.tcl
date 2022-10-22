namespace eval "pkgs/invaders" {
  pkg invaders [list core app] ""

  # /Invaders/Tests% interpreted
  set cflags "-I $::s/pkgs/invaders -I $::s/pkgs/app -I $::s/pkgs/core"
  test $::b/pkgs/invaders/Invaders/tests.tr "$::b/pkgs/core/fble-stdio $::b/pkgs/invaders/Invaders/Tests.fble.d" \
    "$::b/pkgs/core/fble-stdio $cflags -m /Invaders/Tests%"

  app $::b/pkgs/invaders/Invaders/fble-invaders "/Invaders/AppIO%" "invaders"

  # /Invaders/Tests% compiled
  stdio $::b/pkgs/invaders/Invaders/invaders-tests "/Invaders/Tests%" "invaders app"
  test $::b/pkgs/invaders/Invaders/invaders-tests.tr $::b/pkgs/invaders/Invaders/invaders-tests \
    "$::b/pkgs/invaders/Invaders/invaders-tests"
}

