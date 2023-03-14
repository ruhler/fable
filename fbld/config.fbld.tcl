# fbld implementation of @config tag, which let's you refer to 
# config values defined in config.tcl in the build directory.

source config.tcl

# @config[INLINE name]
proc inline_config {name} {
  inline_ [subst "\$::config::$name"]
}
