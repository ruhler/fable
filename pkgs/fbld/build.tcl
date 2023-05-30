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

  # fble-fbld
  stdio $::b/pkgs/fbld/fble-fbld "/Fbld/Main%" "fbld"
  install $::b/pkgs/fbld/fble-fbld $::config::bindir/fble-fbld

  # README.md
  build $::b/pkgs/fbld/README.md \
    "$::b/pkgs/fbld/fble-fbld $::s/README.fbld" \
    "$::b/pkgs/fbld/fble-fbld --markdown --version $::version --buildstamp '' $::s/README.fbld > $::b/pkgs/fbld/README.md"

  # Make sure the version of README.md checked in matches the latest version.
  test $::b/pkgs/fbld/README.tr \
    "$::s/README.md $::b/pkgs/fbld/README.md" \
    "cmp $::s/README.md $::b/pkgs/fbld/README.md"

  # args is additional command line options.
  proc fbld_html_doc { target source args } {
    build $target \
      "$::b/pkgs/fbld/fble-fbld $source $::s/buildstamp" \
      "$::b/pkgs/fbld/fble-fbld $source --html --version $::version --buildstamp `$::s/buildstamp` $args > $target"
  }

  fbld_html_doc $::b/pkgs/fbld/fbld.html $::s/fbld/fbld.fbld
  fbld_html_doc $::b/pkgs/fbld/fble.html $::s/spec/fble.fbld --fble
  fbld_html_doc $::b/pkgs/fbld/README.html $::s/README.fbld
  fbld_html_doc $::b/pkgs/fbld/HelloWorld.html $::s/tutorials/HelloWorld.fbld --tutorial

  # fble-comile.1
  build $::b/pkgs/fbld/fble-compile.1 \
    "$::b/pkgs/fbld/fble-fbld $::s/bin/fble-compile.fbld" \
    "$::b/pkgs/fbld/fble-fbld --man --usage.man --version $::version --buildstamp `$::s/buildstamp` $::s/bin/fble-compile.fbld > $::b/pkgs/fbld/fble-compile.1"
}
