namespace eval "tutorials" {
  # HelloWorld tests
  test $::b/tutorials/HelloWorld.tr \
    "$::b/pkgs/core/fble-stdio $::s/tutorials/HelloWorld/HelloWorld.fble" \
    "$::b/pkgs/core/fble-stdio -I $::s/pkgs/core -I $::s/tutorials/HelloWorld -m /HelloWorld%"

  # Basics tests
  test $::b/tutorials/Basics.tr \
    "$::b/pkgs/core/fble-stdio $::s/tutorials/Basics/Basics.fble" \
    "$::b/pkgs/core/fble-stdio -I $::s/pkgs/core -I $::s/tutorials/Basics -m /Basics%"

  # Modules tests
  test $::b/tutorials/Modules.tr [list \
    $::b/pkgs/core/fble-stdio \
    $::s/tutorials/Modules/Unit.fble \
    $::s/tutorials/Modules/Bit.fble \
    $::s/tutorials/Modules/Bit/Show.fble \
    $::s/tutorials/Modules/Bit4.fble \
    $::s/tutorials/Modules/Bit4/Show.fble \
    $::s/tutorials/Modules/Main.fble] \
    "$::b/pkgs/core/fble-stdio -I $::s/pkgs/core -I $::s/tutorials/Modules -m /Main%"
}
