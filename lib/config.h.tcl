
# Generates a config.h file defining FBLE_CONFIG_* values based on config.tcl

source config.tcl

puts "#ifndef _FBLE_CONFIG_H_"
puts "#define _FBLE_CONFIG_H_"
puts ""

foreach var [lsort [info var ::config::*]] {
  set name [string range $var [string length "::config::"] end]
  set value [subst $$var]
  puts "#define FBLE_CONFIG_[string toupper $name] \"$value\""
}

puts ""
puts "#endif // _FBLE_CONFIG_H_"
