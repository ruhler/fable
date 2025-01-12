namespace eval "pkgs/app" {

  # .c library files
  set objs [list]
  foreach {x} { sockets.fble } {
    lappend objs $::b/pkgs/network/$x.o
    obj $::b/pkgs/network/$x.o $::s/pkgs/network/$x.c \
      "-I $::s/include -I $::s/pkgs/core"
  }
  pkg network [list core] "" $objs

  # Check doc comments
  foreach {x} [build_glob $::s/pkgs/network -tails "*.h" "*.c"] {
    fbld_check_dc $::b/pkgs/network/$x.dc $::s/pkgs/network/$x
  }

  # fble-hello-client program.
  # TODO: Rename this fble-network-client or some such and install it.
  stdio $::b/pkgs/network/fble-hello-client "/Network/Sockets/Hello%" "network" ""

  # fble-network-echo program.
  stdio $::b/pkgs/network/fble-network-echo "/Network/Echo%" "network" ""
  install $::b/pkgs/network/fble-network-echo $::config::bindir/fble-network-echo
}
