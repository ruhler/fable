# Tcl-based implementation of Fbld.

namespace eval fbld {
  # Removes backslash from backslash-escaped \, @, [, and ] characters in a
  # string.
  proc unescape { text } {
    return [string map { \\\\ \\ \\@ @ \\[ [ \\] ] } $text]
  }

  # Returns the index of the first un-escaped '@' character in 'text',
  # starting at 'start' index. If no '@' character is found, returns the length
  # of 'text'.
  #
  # This is used for parsing unadorned text between commands from inline
  # structured text.
  proc find_inline_command { text start } {
    set len [string length $text]
    for {set i $start} {$i < $len} { incr i } {
      switch -exact [string index $text $i] {
        \\ { incr i }
        @ { return $i }
      }
    }
    return $len
  }

  # Returns the index of the ']' character closing the current argument,
  # starting from 'start'. The initial '[' character for the argument is assumed
  # to have come from before 'start' index.
  proc find_arg_end { text start } {
    set nest 0
    set len [string length $text]
    for {set i $start} {$i < $len} { incr i } {
      switch -exact [string index $text $i] {
        \\ { incr i }
        \[ { incr nest }
        \] {
          if {$nest == 0} {
            return $i
          }
          incr nest -1
        }
      }
    }
    error "Unterminated argument starting at index $start in text '$text'"
  }

  proc is_command_name_char { char } {
    return [string match {[A-Za-z0-9_]} $char]
  }

  proc is_space_or_tab { char } {
    return [expr [string equal " " $char] || [string equal "\t" $char]]
  }

  # Parses inline structured text.
  # @arg invoke
  #   The name of a function for invoking commands. The first argument to the
  #   function is the name of the command, or "" for the default command.
  #   Subsequent arguments are the arguments passed to the command.
  # @arg text  The text to parse.
  proc inline { invoke text } {
    set len [string length $text]
    set i 0
    while {$i < $len} {
      # Check for plain text before the next @... command and invoke the default
      # command as appropriate.
      set cmd_start [find_inline_command $text $i]
      if {$cmd_start > $i} {
        set arg [string range $text $i [expr $cmd_start - 1]]
        $invoke "" $arg
      }
      set i $cmd_start

      if {$i == $len} {
        return;
      }

      # Parse the name of this @... command
      incr i
      set name_end $i
      while {[is_command_name_char [string index $text $name_end]]} {
        incr name_end
      }
      if {$name_end == $i} {
        error "Missing command name at index $i in text '$text'"
      }
      set cmd [string range $text $i [expr $name_end - 1]]
      set i $name_end

      # Parse arguments to the @... command.
      set args [list]
      while {[string equal [string index $text $i] \[]} {
        incr i
        set arg_end [find_arg_end $text $i]
        set arg [string range $text $i [expr $arg_end - 1]]
        lappend args $arg
        set i $arg_end
        incr i
      }

      $invoke $cmd {*}$args
    }
  }

  # Parses block structured text.
  # @arg invoke
  #   The name of a function for invoking commands. The first argument to the
  #   function is the name of the command, or "" for the default command.
  #   Subsequent arguments are the arguments passed to the command.
  # @arg text  The text to parse.
  proc block { invoke text } {
    set len [string length $text]
    set i 0
    while {$i < $len} {
      # Skip past any initial blank lines.
      while {[string equal [string index $text $i] \n]} {
        incr i
      }

      if {$i == $len} {
        return
      }

      # Check for any plain text occuring before the next command and invoke
      # the default command on it as appropriate.
      if {![string equal [string index $text $i] @]} {
        set arg_end [string first "\n\n" $text $i]
        if {$arg_end == -1} {
          set arg_end [expr $len - 1]
        }
        set arg [string range $text $i $arg_end]
        $invoke "" $arg
        set i $arg_end
        incr i
        continue
      }

      # Parse the name of this @... command
      incr i
      set name_end $i
      while {[is_command_name_char [string index $text $name_end]]} {
        incr name_end
      }
      if {$name_end == $i} {
        error "Missing command name at index $i in text '$text'"
      }
      set cmd [string range $text $i [expr $name_end - 1]]
      set i $name_end

      # Parse explicit arguments to the @... command.
      set args [list]
      while {[string equal [string index $text $i] \[]} {
        incr i
        set arg_end [find_arg_end $text $i]
        set arg [string range $text $i [expr $arg_end - 1]]
        lappend args $arg
        set i $arg_end
        incr i
      }

      # Check for end of file.
      if {$i >= $len} {
        $invoke $cmd {*}$args
        return
      }

      # Extract the same line arg.
      set arg_end [string first "\n" $text $i]
      if {$arg_end == -1} {
        set arg_end $len
      }
      set arg [string range $text $i [expr $arg_end - 1]]
      if {![string equal "" $arg]} {
        lappend args [string range $arg 1 end]
      }
      set i $arg_end
      incr i

      # Extract next line arg.
      set arg ""
      while {$i < $len} {
        if {[string equal "\n" [string index $text $i]]} {
          append arg "\n"
          incr i
          continue
        }

        if [string equal " " [string index $text $i]] {
          incr i
          set arg_end [string first "\n" $text $i]
          if {$arg_end == -1} {
            set arg_end $len
          }
          append arg [string range $text $i $arg_end]
          set i $arg_end
          incr i
          continue
        }

        break;
      }

      # Remove any trailing blank lines.
      while {[string equal "\n\n" [string range $arg end-1 end]]} {
        set arg [string range $arg 0 end-1]
      }

      if {![string equal "" $arg] && ![string equal "\n" $arg]} {
        lappend args $arg
      }

      $invoke $cmd {*}$args
    }
  }

  namespace export unescape inline block
}
