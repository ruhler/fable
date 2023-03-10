# Ad-hoc tests for fbld.tcl implementation.

set fblddir [file dirname [file normalize [info script]]]
source $fblddir/fbld.tcl

proc invoke { cmd args } {
  lappend ::invoked [list $cmd $args]
}

# Test parsing of fbld.
# @arg fbld  Either "inline" or "block".
# @arg text  The text to parse
# @arg expected A script that manually invokes the expected commands using the
#               invoke procedure, for comparison against what fbld invokes.
proc test { fbld text expected } {
  set ::invoked [list]
  eval $expected
  set want $::invoked

  set ::invoked [list]
  ::fbld::$fbld invoke $text
  set got $::invoked

  if {![string equal $want $got]} {
    error "EXPECTED:\n$want\nGOT:\n$got\n"
  }
}

# Basic parsing of inline structured text.
test inline {Inline @emph[text] with @SINGLE and @multi[abc][def] arg commands} {
  invoke "" "Inline "
  invoke emph "text"
  invoke "" " with "
  invoke SINGLE
  invoke "" " and "
  invoke multi "abc" "def"
  invoke "" " arg commands"
}

# Nested commands
test inline {Ab @c[de @f[gh] ij] kl} {
  invoke "" "Ab "
  invoke c {de @f[gh] ij}
  invoke "" " kl"
}

# Basic parsing of block structured text.
test block {
@title A title

Some intro text.

@section[First Section]
 With some text
 spanning over lines.

 multiple lines actually.

And some final text.
} {
  invoke title "A title"
  invoke "" "Some intro text.\n"
  invoke section "First Section" "With some text\nspanning over lines.\n\nmultiple lines actually.\n"
  invoke "" "And some final text.\n"
}

# Regression test for a default block that runs to the end of the file.
test block "Foo" {
  invoke "" "Foo"
}

