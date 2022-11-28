# Ad-hoc tests for fbld.tcl implementation.

source fbld.tcl

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

# Basic parsing of block structured text.
test inline {Inline @emph[text] with @SINGLE and @multi[abc][def] arg commands} {
  invoke "" "Inline "
  invoke emph "text"
  invoke "" " with "
  invoke SINGLE
  invoke "" " and "
  invoke multi "abc" "def"
  invoke "" " arg commands"
}

# Basic parsing of block structured text.
test block {
@title A title

Some intro text.

@section[First Section]
With some text
spanning over lines.

And some final text.
} {
  invoke title "A title"
  invoke "" "Some intro text.\n"
  invoke section "First Section" "With some text\nspanning over lines.\n"
  invoke "" "And some final text.\n"
}

# Test end of file after the last ]
test block {
@foo[A]
[
B]} {
  invoke foo "A" "\nB"
}
