# Helper script to better understand cost of dependencies that the fble
# project has.
#
# Usage: tclsh8.6 deps.tcl [PACKAGE ...]
#
# The output is suitable to pass as input to the fble-perf-profile tool for
# viewing.
#
# Requires apt-rdepends is installed, and that all relevant packages are
# installed.

# Parse the dependency graph.
# Format is, for example:
# doxygen
#   Depends: libc6 (>= 2.17)
#   Depends: libclang1-6.0 (>= 1:5.0~svn298832-1~)
# libc6
#   Depends: libgcc1
set curr ""
set rdepends [exec -ignorestderr apt-rdepends --state-show=Installed --state-follow=Installed {*}$argv]
foreach line [split $rdepends "\n"] {
  if {[string first "Depends:" $line] != -1} {
    set dep [lindex [split [string trim $line]] 1]
    lappend ::deps($curr) $dep
  } else {
    set curr [string trim $line]
    set ::deps($curr) [list]
    set ::size($curr) [exec dpkg-query -Wf {${Installed-Size}} $curr]
  }
}

# Returns the common prefix between two lists.
proc common_prefix {a b} {
  set prefix [list]
  set al [llength $a]
  set bl [llength $b]
  set i 0
  while {$i < $al && $i < $bl} {
    if {[string equal [lindex $a $i] [lindex $b $i]]} {
      lappend prefix [lindex $a $i]
    } else {
      return $prefix
    }
    incr i
  }
  return $prefix
}

# Compute the dominators for each package.
# @param path  - The path from the root
# @param pkgs  - The list of packages to traverse
proc dfs {path pkgs} {
  foreach pkg $pkgs {
    # puts "PATH: $path $pkg"
    if {[lsearch -exact $path $pkg] != -1} {
      # libc depends on gcc depends on libc?
      continue
    }

    if {[string equal "" [array names ::dom -exact $pkg]]} {
      # First time we have seen the package.
      set ::dom($pkg) $path
    } else {
      set ::dom($pkg) [common_prefix $::dom($pkg) $path]
    }
    # puts "DOM: $pkg: $::dom($pkg)"

    dfs [concat $path $pkg] $::deps($pkg)
  }
}

dfs [list] $argv

# Print out the results in a format suitable for input to fble-perf-profile
puts "# Package dominators for $argv"
foreach pkg [array names ::size] {
  puts "$::size($pkg) [join [concat $::dom($pkg) $pkg] ";"]"
}
