namespace eval "pkgs/invaders" {
  pkg invaders [list core app] ""
  app $::out/pkgs/invaders/Invaders/fble-invaders "/Invaders/App%" \
    "fble-invaders" $::out/pkgs/invaders/libfble-invaders.a

  # /Invaders/Tests% interpreted
  set cflags "-I pkgs/invaders -I pkgs/app -I pkgs/core"
  test $::out/pkgs/invaders/Invaders/tests.tr "$::out/pkgs/core/Core/fble-stdio $::out/pkgs/invaders/Invaders/Tests.fble.d" \
    "$::out/pkgs/core/Core/fble-stdio $cflags -m /Invaders/Tests%" "pool = console"

  # /Invaders/Tests% compiled
  stdio $::out/pkgs/invaders/Invaders/invaders-tests "/Invaders/Tests%" "fble-invaders" \
    "$::out/pkgs/core/libfble-core.a $::out/pkgs/app/libfble-app.a $::out/pkgs/invaders/libfble-invaders.a"
  test $::out/pkgs/invaders/Invaders/invaders-tests.tr $::out/pkgs/invaders/Invaders/invaders-tests \
    "$::out/pkgs/invaders/Invaders/invaders-tests" "pool = console"
}

