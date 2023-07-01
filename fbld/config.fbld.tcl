
# Generates a config.fbld file defining @config command based on config.tcl

source config.tcl

puts {@defi[config][name]}
puts { @switch[$name]}

foreach var [lsort [info var ::config::*]] {
  set name [string range $var [string length "::config::"] end]
  set value [subst $$var]
  puts " @\[$name\] $value"
}
puts { @ @error[Invalid config option: $name]}
puts {@@}
