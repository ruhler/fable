namespace eval "pkgs/hwdg" {
  dist_s $::s/pkgs/hwdg/build.tcl
  dist_s $::s/pkgs/hwdg/TODO.txt

  pkg hwdg [list core app] ""

  # /Hwdg/Tests% interpreted
  set cflags "-I $::s/pkgs/hwdg -I $::s/pkgs/app -I $::s/pkgs/core"
  testsuite $::b/pkgs/hwdg/Hwdg/tests.tr "$::b/pkgs/core/fble-stdio $::b/pkgs/hwdg/Hwdg/Tests.fble.d" \
    "$::b/pkgs/core/fble-stdio $cflags -m /Hwdg/Tests% --prefix Interpreted."

  # /Hwdg/Tests% compiled
  stdio $::b/pkgs/hwdg/Hwdg/hwdg-tests "/Hwdg/Tests%" "hwdg app"
  testsuite $::b/pkgs/hwdg/Hwdg/hwdg-tests.tr $::b/pkgs/hwdg/Hwdg/hwdg-tests \
    "$::b/pkgs/hwdg/Hwdg/hwdg-tests --prefix Compiled."
}
