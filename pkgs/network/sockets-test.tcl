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
exec $echo -p 9007 -n 1 &

puts "Launching $client..."
set output [exec $client -p 9007 -m hello -n 5]

if {![string equal $output "hello"]} {
  error "expected hello, got $output"
}
exit 0
