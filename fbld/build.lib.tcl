# fbld implementation of some build related tags.

source config.tcl

# @config[INLINE name]
# Outputs the value of the named build config parameter.
proc inline_config {name} {
  inline_ [subst "\$::config::$name"]
}

# @BuildStamp
# Outputs the build stamp for the document.
proc inline_BuildStamp {} {
  inline_ [exec $::config::srcdir/buildstamp]
}
