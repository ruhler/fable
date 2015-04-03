
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
  proc $name.fields {} "return [list $fields]"
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
  error "Tag out of value, or not enough args"
}

proc . {obj field} {
  set i 1
  foreach objfield [[typeof $obj].fields] {
    if {[lindex $objfield 1]==$field} {
      return [lindex $obj $i]
    }
    incr i
  }
  error "$obj with fields [[typeof $obj].fields] has no field $field. "
}


struct Unit {}
union Bit {{Unit 0} {Unit 1}}
struct FullAdderOut {{Bit z} {Bit cout}}

set b0 [Bit 0 [Unit]]
set b1 [Bit 1 [Unit]]

funct FullAdder {{Bit a} {Bit b} {Bit cin}} FullAdderOut {
  global b0 b1
  let Bit z = [? $a [? $b $cin [? $cin $b1 $b0]] [? $b [? $cin $b1 $b0] $cin]]
  let Bit cout = [? $a [? $b $b0 $cin] [? $b $cin $b1]]
  return [FullAdderOut $z $cout]
}

puts [FullAdder $b0 $b1 $b0]
puts [FullAdder $b0 $b1 $b1]

struct Bit4 {{Bit 0} {Bit 1} {Bit 2} {Bit 3}}
struct AdderOut {{Bit4 z} {Bit cout}}
funct Adder {{Bit4 a} {Bit4 b} {Bit cin}} AdderOut {
  let FullAdderOut x0 = [FullAdder [. $a 0] [. $b 0] $cin]
  let FullAdderOut x1 = [FullAdder [. $a 1] [. $b 1] [. $x0 cout]]
  let FullAdderOut x2 = [FullAdder [. $a 2] [. $b 2] [. $x1 cout]]
  let FullAdderOut x3 = [FullAdder [. $a 3] [. $b 3] [. $x2 cout]]
  return [AdderOut [Bit4 [. $x0 z] [. $x1 z] [. $x2 z] [. $x3 z]] [. $x3 cout]]
}

puts [Adder [Bit4 $b0 $b1 $b0 $b0] [Bit4 $b0 $b1 $b1 $b0] $b0]

