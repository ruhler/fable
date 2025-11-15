namespace eval "pkgs/computrainer" {
  pkg computrainer [list core app] "" ""

  if $::config::enable_fble_app {
    app $::b/pkgs/computrainer/computrainer-bin "/Computrainer/Main%" "computrainer"
    install $::b/pkgs/computrainer/computrainer-bin $::config::bindir/computrainer
  }
}
