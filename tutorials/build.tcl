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

  # Tutorial table of contents
  ::html_doc $::b/www/tutorials/Tutorials.html $::s/tutorials/Tutorials.fbld
  www $::b/www/tutorials/Tutorials.html

  # Tutorials
  foreach {x} [build_glob $::s/tutorials *.fbld] {
    if [string equal $x $::s/tutorials/Tutorials.fbld] {
      continue
    }

    set base [file rootname [file tail $x]]
    ::html_tutorial $::b/www/tutorials/$base.html $::s/tutorials/$base.fbld
    www $::b/www/tutorials/$base.html
  }
}
