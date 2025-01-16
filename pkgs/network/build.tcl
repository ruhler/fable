namespace eval "pkgs/app" {

  # .c library files
  set objs [list]
  foreach {x} { sockets.fble } {
    lappend objs $::b/pkgs/network/$x.o
    obj $::b/pkgs/network/$x.o $::s/pkgs/network/$x.c \
      "-I $::s/include -I $::s/pkgs/core"
  }

  set usages [list \
    $::b/pkgs/network/Network/Apps/Client/Usage.fble \
    $::b/pkgs/network/Network/Apps/Echo/Usage.fble \
  ]

  pkg network [list core] $usages $objs

  # Check doc comments
  foreach {x} [build_glob $::s/pkgs/network -tails "*.h" "*.c"] {
    fbld_check_dc $::b/pkgs/network/$x.dc $::s/pkgs/network/$x
  }

  set ldflags ""
  if {[string first "_NT" [exec uname -s]] != -1} {
    # Windows needs to link to Ws2_32.lib for the sockets library.
    set ldflags "-lWs2_32"
  }

  # fble-network-client program.
  fbld_help_fble_usage $::b/pkgs/network/Network/Apps/Client/Usage.fble $::s/pkgs/network/fble-network-client.fbld
  fbld_man_usage $::b/pkgs/network/fble-network-client.1 $::s/pkgs/network/fble-network-client.fbld
  install $::b/pkgs/network/fble-network-client.1 $::config::mandir/man1/fble-network-client.1
  stdio $::b/pkgs/network/fble-network-client "/Network/Apps/Client%" "network" $ldflags
  install $::b/pkgs/network/fble-network-client $::config::bindir/fble-network-client

  # fble-network-echo program.
  fbld_help_fble_usage $::b/pkgs/network/Network/Apps/Echo/Usage.fble $::s/pkgs/network/fble-network-echo.fbld
  fbld_man_usage $::b/pkgs/network/fble-network-echo.1 $::s/pkgs/network/fble-network-echo.fbld
  install $::b/pkgs/network/fble-network-echo.1 $::config::mandir/man1/fble-network-echo.1
  stdio $::b/pkgs/network/fble-network-echo "/Network/Apps/Echo%" "network" $ldflags
  install $::b/pkgs/network/fble-network-echo $::config::bindir/fble-network-echo
}
