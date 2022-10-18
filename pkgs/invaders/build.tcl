namespace eval "pkgs/invaders" {
  pkg invaders [list core app] ""

  # /Invaders/Tests% interpreted
  set cflags "-I pkgs/invaders -I pkgs/app -I pkgs/core"
  test $::out/pkgs/invaders/Invaders/tests.tr "$::out/pkgs/core/Core/fble-stdio $::out/pkgs/invaders/Invaders/Tests.fble.d" \
    "$::out/pkgs/core/Core/fble-stdio $cflags -m /Invaders/Tests%" "pool = console"

  app $::out/pkgs/invaders/Invaders/fble-invaders "/Invaders/AppIO%" "invaders"

  # /Invaders/Tests% compiled
  stdio $::out/pkgs/invaders/Invaders/invaders-tests "/Invaders/Tests%" "invaders app"
  test $::out/pkgs/invaders/Invaders/invaders-tests.tr $::out/pkgs/invaders/Invaders/invaders-tests \
    "$::out/pkgs/invaders/Invaders/invaders-tests" "pool = console"
}

