# Ad-hoc tests for fbld.tcl implementation.

source fbld.tcl

proc test_invoke { cmd args } {
  puts "cmd: '$cmd'"
  set i 0
  foreach arg $args {
    puts "arg $i: {$arg}"
    incr i
  }
}

inline test_invoke {
It's @emph[really] important to check the result of the @code[malloc] function
in C, in case it returns @NULL. See
@link[the malloc man page][https://manpages.org/malloc] for more details.
}

