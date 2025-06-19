# sockets-test.tcl
#
# Script to test sockets.
#
# Usage:
#   tclsh8.6 fble-network-echo fble-network-client
#
#   fble-network-echo: Path to fble-network-echo binary.
#   fble-network-client: Path to fble-network-client binary.

set echo [lindex $argv 0]
set client [lindex $argv 1]

puts "Launching $echo..."
set server_pid [exec $echo -p 9007 -n 1 &]

try {
  # Give some time for the server to launch before launching the client to
  # reduce the chance of the client racing ahead.
  after 1000

  puts "Launching $client..."
  set output [exec $client -p 9007 -m hello -n 5]

  if {![string equal $output "hello"]} {
    error "expected hello, got $output"
  }
  exit 0
} finally {
  # Prevent deadlock in case the client never manages to connect with the
  # server.
  exec kill $server_pid
}
