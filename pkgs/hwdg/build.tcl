namespace eval "pkgs/hwdg" {
  pkg hwdg [list core app] ""

  # /Hwdg/Tests% interpreted
  set cflags "-I pkgs/hwdg -I pkgs/app -I pkgs/core"
  test $::out/pkgs/hwdg/Hwdg/tests.tr "$::out/pkgs/core/Core/fble-stdio $::out/pkgs/hwdg/Hwdg/Tests.fble.d" \
    "$::out/pkgs/core/Core/fble-stdio $cflags -m /Hwdg/Tests%" "pool = console"

  # /Hwdg/Tests% compiled
  stdio $::out/pkgs/hwdg/Hwdg/hwdg-tests "/Hwdg/Tests%" "fble-hwdg" \
    "$::out/pkgs/core/libfble-core.a $::out/pkgs/hwdg/libfble-hwdg.a"
  test $::out/pkgs/hwdg/Hwdg/hwdg-tests.tr $::out/pkgs/hwdg/Hwdg/hwdg-tests \
    "$::out/pkgs/hwdg/Hwdg/hwdg-tests" "pool = console"
}
