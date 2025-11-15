namespace eval "pkgs/computrainer" {
  pkg computrainer [list core app] "" ""

  # Computrainer/Tests compiled
  # --allow-shlib-undefined because we know this doesn't use the part of the
  # app package that depends on SDL.
  stdio $::b/pkgs/computrainer/computrainer-tests "/Computrainer/Tests%" "app computrainer" "-Wl,--allow-shlib-undefined"
  testsuite $::b/pkgs/computrainer/computrainer-tests.tr $::b/pkgs/computrainer/computrainer-tests \
    "$::b/pkgs/computrainer/computrainer-tests"

  if $::config::enable_fble_app {
    app $::b/pkgs/computrainer/computrainer-bin "/Computrainer/Main%" "computrainer"
    install $::b/pkgs/computrainer/computrainer-bin $::config::bindir/computrainer
  }
}
