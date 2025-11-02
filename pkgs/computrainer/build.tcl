namespace eval "pkgs/computrainer" {
  pkg computrainer [list core app] "" ""

  if $::config::enable_fble_app {
    app $::b/pkgs/computrainer/computrainer "/Computrainer%" "computrainer"
    install $::b/pkgs/computrainer/computrainer $::config::bindir/computrainer
  }
}
