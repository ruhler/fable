
proc typeof {value} {
  return [lindex $value 0]
}

proc tagof {value} {
  return [lindex $value 1]
}

proc struct {name fields} {
  # TODO: Check the types of the arguments when the constructor is called.
  set fieldnames [list]
  set listcmd "list $name"
  foreach a $fields {
    set fieldname [lindex $a 1]
    lappend fieldnames $fieldname
    set listcmd "$listcmd \$[lindex $fieldname]"
  }
  proc $name $fieldnames "return \[$listcmd\]"
}

proc union {name fields} {
  # TODO:
  #  * Check the type of the argument when the constructor is called.
  proc $name {tag value} "return \[list $name \$tag \$value\]"
  proc $name.fields {} "return [list $fields]"
}

proc funct {name args out_type body} {
  # TODO:
  #  * Check the type of the arguments and return value.
  set argnames [list]
  foreach a $args {
    lappend argnames [lindex $a 1]
  }
  proc $name $argnames $body
}

# TODO: Check the type when doing the assignment.
# TODO: Verify equals is '='.
proc let {type name equals value} {
  upvar $name $name
  set $name $value
}

proc ? {select args} {
  set i 0
  foreach field [[typeof $select].fields] {
    if {[lindex $field 1]==[tagof $select]} {
      return [lindex $args $i]
    }
    incr i
  }
}


struct Unit {}
union Bit {{Unit 0} {Unit 1}}
struct FullAdderOut {{Bit z} {Bit cout}}

funct FullAdder {{Bit a} {Bit b} {Bit cin}} FullAdderOut {
  let Bit b0 = [Bit 0 [Unit]]
  let Bit b1 = [Bit 1 [Unit]]
  let Bit z = [? $a [? $b $cin [? $cin $b1 $b0]] [? $b [? $cin $b1 $b0] $cin]]
  let Bit cout = [? $a [? $b $b0 $cin] [? $b $cin $b1]]
  return [FullAdderOut $z $cout]
}

puts [FullAdder [Bit 0 [Unit]] [Bit 1 [Unit]] [Bit 0 [Unit]]]
puts [FullAdder [Bit 0 [Unit]] [Bit 1 [Unit]] [Bit 1 [Unit]]]

