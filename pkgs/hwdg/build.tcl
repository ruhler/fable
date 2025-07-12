namespace eval "pkgs/hwdg" {
  pkg hwdg [list core app] "" ""

  # /Hwdg/Tests% interpreted
  set cflags "-I $::s/pkgs/hwdg -I $::s/pkgs/app -I $::s/pkgs/core"
  run_stdio_tests $::b/pkgs/hwdg/Hwdg/tests.tr "$cflags -m /Hwdg/Tests%" ""

  # /Hwdg/Tests% compiled
  # --allow-shlib-undefined because we know this doesn't use the part of the
  # app package that depends on SDL.
  stdio $::b/pkgs/hwdg/Hwdg/hwdg-tests "/Hwdg/Tests%" "app hwdg" "-Wl,--allow-shlib-undefined"
  testsuite $::b/pkgs/hwdg/Hwdg/hwdg-tests.tr $::b/pkgs/hwdg/Hwdg/hwdg-tests \
    "$::b/pkgs/hwdg/Hwdg/hwdg-tests --prefix Compiled."
}
