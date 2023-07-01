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

  # README.md
  build $::b/pkgs/fbld/README.md \
    "$::b/pkgs/fbld/fbld $::s/fbld/nobuildstamp.fbld $::b/fbld/version.fbld $::s/README.fbld" \
    "$::b/pkgs/fbld/fbld --markdown $::s/fbld/nobuildstamp.fbld $::b/fbld/version.fbld $::s/README.fbld > $::b/pkgs/fbld/README.md"

  # Make sure the version of README.md checked in matches the latest version.
  test $::b/pkgs/fbld/README.tr \
    "$::s/README.md $::b/pkgs/fbld/README.md" \
    "cmp $::s/README.md $::b/pkgs/fbld/README.md"

  # args is additional command line options.
  proc fbld_html_doc { target sources args } {
    build $target \
      "$::b/pkgs/fbld/fbld $::b/fbld/version.fbld $sources $::s/buildstamp" \
      "$::s/buildstamp --fbld BuildStamp | $::b/pkgs/fbld/fbld - $::b/fbld/version.fbld $sources --html $args > $target"
  }

  fbld_html_doc $::b/pkgs/fbld/fbld.html $::s/fbld/fbld.fbld
  fbld_html_doc $::b/pkgs/fbld/fble.html "$::s/spec/fble.lib.fbld $::s/spec/fble.fbld"
  fbld_html_doc $::b/pkgs/fbld/README.html $::s/README.fbld
  fbld_html_doc $::b/pkgs/fbld/HelloWorld.html "$::s/tutorials/tutorial.lib.fbld $::s/tutorials/HelloWorld.fbld"

  www $::b/pkgs/fbld/fbld.html
  www $::b/pkgs/fbld/fble.html
  www $::b/pkgs/fbld/README.html
  www $::b/pkgs/fbld/HelloWorld.html

  # fble-comile.1
  build $::b/pkgs/fbld/fble-compile.1 \
    "$::b/pkgs/fbld/fbld $::b/fbld/version.fbld $::b/fbld/config.fbld $::s/fbld/usage.man.fbld $::s/fbld/usage.lib.fbld $::s/bin/fble-compile.fbld" \
    "$::s/buildstamp --fbld BuildStamp | $::b/pkgs/fbld/fbld --text - $::b/fbld/version.fbld $::s/fbld/usage.man.fbld $::s/fbld/usage.lib.fbld $::s/bin/fble-compile.fbld > $::b/pkgs/fbld/fble-compile.1"
}
