namespace eval "pkgs/graphics" {
  pkg graphics [list core app] ""

  # Usage.fble is checked in from generated code. Add a test to ensure
  # the checked in version is updated. If this test fails, update the source
  # version of Usage.fble to match the generated version.
  fbld_help_fble_usage $::b/pkgs/graphics/Graphics/Usage.fble $::s/pkgs/graphics/fble-graphics.fbld
  test $::b/pkgs/graphics/Usage.tr \
    "$::s/pkgs/graphics/Graphics/Usage.fble $::b/pkgs/graphics/Graphics/Usage.fble" \
    "diff --strip-trailing-cr $::s/pkgs/graphics/Graphics/Usage.fble $::b/pkgs/graphics/Graphics/Usage.fble"

  # /Graphics/Tests% interpreted
  set cflags "-I $::s/pkgs/graphics -I $::s/pkgs/app -I $::s/pkgs/core"
  testsuite $::b/pkgs/graphics/tests-interpreted.tr "$::b/pkgs/core/fble-stdio $::b/pkgs/graphics/Graphics/Tests.fble.d" \
    "$::b/pkgs/core/fble-stdio $cflags -m /Graphics/Tests%"

  if $::config::enable_fble_app {
    fbld_man_usage $::b/pkgs/graphics/fble-graphics.1 $::s/pkgs/graphics/fble-graphics.fbld
    install $::b/pkgs/graphics/fble-graphics.1 $::config::mandir/man1/fble-graphics.1
    app $::b/pkgs/graphics/fble-graphics "/Graphics/AppIO%" "graphics"
    install $::b/pkgs/graphics/fble-graphics $::config::bindir/fble-graphics
  }
}
