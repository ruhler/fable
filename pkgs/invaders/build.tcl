namespace eval "pkgs/invaders" {
  pkg invaders ""
  app $::out/pkgs/invaders/Invaders/fble-invaders "/Invaders/App%" \
    "fble-invaders" $::out/pkgs/invaders/libfble-invaders.a

  # /Invaders/Tests% interpreted
  test $::out/pkgs/invaders/Invaders/tests.tr "$::out/pkgs/core/Core/fble-stdio $::out/pkgs/invaders/Invaders/Tests.fble.d" \
    "$::out/pkgs/core/Core/fble-stdio [exec pkg-config --cflags-only-I fble-invaders] -m /Invaders/Tests%" "pool = console"

  # /Invaders/Tests% compiled
  stdio $::out/pkgs/invaders/Invaders/invaders-tests "/Invaders/Tests%" "fble-invaders" \
    "$::out/pkgs/core/libfble-core.a $::out/pkgs/app/libfble-app.a $::out/pkgs/invaders/libfble-invaders.a"
  test $::out/pkgs/invaders/Invaders/invaders-tests.tr $::out/pkgs/invaders/Invaders/invaders-tests \
    "$::out/pkgs/invaders/Invaders/invaders-tests" "pool = console"
}

