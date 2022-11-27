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

puts "INLINE TEST INVOKE"
inline test_invoke {
It's @emph[really] important to check the result of the @code[malloc] function
in C, in case it returns @NULL. See
@link[the malloc man page][https://manpages.org/malloc] for more details.
}

puts "BLOCK TEST INVOKE"
block test_invoke {
@title A Bigger Example

Some introductory text here.

@section[First Section]
[
@paragraph
Here is a paragraph in the first
section of the doc.

@list
@item First item of a list.
@item Second item of a list.
]
}

puts "BLOCK TEST INVOKE 2"
block test_invoke {
@paragraph
Here is a paragraph in the first
section of the doc.

@list
@item First item of a list.
@item Second item of a list.
}

puts "BLOCK TEST INVOKE 3"
block test_invoke {
@item First item of a list.
@item Second item of a list.
}
