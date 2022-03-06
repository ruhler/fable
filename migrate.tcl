
# Temporary helper script to help migrate spec tests from old format to new
# format.
# TODO: remove this when migration is complete.

proc dirs { root dir } {
  set l [list $dir]
  foreach {x} [glob -tails -directory $root -nocomplain -type d $dir*] {
    set l [concat $l [dirs $root "$x/"]]
  }
  return $l
}

proc files { root } {
  set xs [list]
  foreach dir [dirs $root ""] {
    foreach {x} [glob -tails -directory $root -nocomplain -type f $dir/*.tcl] {
      lappend xs $x
    }
  }
  return $xs
}

proc renamed { path } {
  set dirs [list]
  foreach d [split [file rootname $path] "/"] {
    set words [list]
    foreach word [split $d "-"] {
      lappend words [string toupper $word 0 0]
    }
    lappend dirs [join $words ""]
  }
  return "[join $dirs "/"].fble"
}

set dir 4.4-union-select
set ddir 4.4-UnionSelect

foreach {x} [files langs/fble/$dir] {
  set y [renamed $x]
  set src langs/fble/$dir/$x
  set dst spec/SpecTests/$ddir/$y
  exec mkdir -p [file dirname $dst]
  exec git mv $src $dst
}

