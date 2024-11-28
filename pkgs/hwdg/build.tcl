namespace eval "pkgs/hwdg" {
  pkg hwdg [list core app] "" ""

  # /Hwdg/Tests% interpreted
  set cflags "-I $::s/pkgs/hwdg -I $::s/pkgs/app -I $::s/pkgs/core"
  testsuite $::b/pkgs/hwdg/Hwdg/tests.tr "$::b/pkgs/core/fble-stdio $::b/pkgs/hwdg/Hwdg/Tests.fble.d" \
    "$::b/pkgs/core/fble-stdio $cflags -m /Hwdg/Tests% --prefix Interpreted."

  # /Hwdg/Tests% compiled
  # --allow-shlib-undefined because we know this doesn't use the part of the
  # app package that depends on SDL.
  stdio $::b/pkgs/hwdg/Hwdg/hwdg-tests "/Hwdg/Tests%" "app hwdg" "-Wl,--allow-shlib-undefined"
  testsuite $::b/pkgs/hwdg/Hwdg/hwdg-tests.tr $::b/pkgs/hwdg/Hwdg/hwdg-tests \
    "$::b/pkgs/hwdg/Hwdg/hwdg-tests --prefix Compiled."
}
