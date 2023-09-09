namespace eval "pkgs/fbld" {
  # /Fbld/Fble/Version/Value%
  build $::b/pkgs/fbld/Fbld/Fble/Version/Value.fble "" \
    "echo \"/Core/String%.Str|'$::version';\" > $::b/pkgs/fbld/Fbld/Fble/Version/Value.fble"

  pkg fbld [list core] ""

  # Fbld/Tests interpreted
  testsuite $::b/pkgs/fbld/Fbld/tests.tr "$::b/pkgs/core/fble-stdio $::b/pkgs/fbld/Fbld/Tests.fble.d" \
    "$::b/pkgs/core/fble-stdio -I $::s/pkgs/core -I $::s/pkgs/fbld -m /Fbld/Tests% --prefix Interpreted."

  # Fbld/Tests compiled
  stdio $::b/pkgs/fbld/fbld-tests "/Fbld/Tests%" "fbld"
  testsuite $::b/pkgs/fbld/fbld-tests.tr $::b/pkgs/fbld/fbld-tests \
    "$::b/pkgs/fbld/fbld-tests --prefix Compiled."

  # fbld
  stdio $::b/pkgs/fbld/fbld "/Fbld/Main%" "fbld"
  install $::b/pkgs/fbld/fbld $::config::bindir/fbld

  # args is additional command line options.
  proc fbld_html_doc { target sources args } {
    build $target \
      "$::b/pkgs/fbld/fbld $::b/fbld/version.fbld $sources $::s/buildstamp" \
      "$::s/buildstamp --fbld BuildStamp | $::b/pkgs/fbld/fbld - $::b/fbld/version.fbld $sources --html $args > $target"
  }

  fbld_html_doc $::b/pkgs/fbld/fble.html "$::s/spec/fble.lib.fbld $::s/spec/fble.fbld"
  fbld_html_doc $::b/pkgs/fbld/HelloWorld.html "$::s/tutorials/tutorial.lib.fbld $::s/tutorials/HelloWorld.fbld"

  www $::b/pkgs/fbld/fble.html
  www $::b/pkgs/fbld/HelloWorld.html
}
