namespace eval "tutorials" {
  set tutorials {
    AbstractTypes.fbld
    Basics.fbld
    ByComparison.fbld
    Bind.fbld
    Core.fbld
    Functions.fbld
    HelloWorld.fbld
    HelloWorldRevisited.fbld
    Install.fbld
    Introduction.fbld
    Lists.fbld
    Literals.fbld
    Modules.fbld
    Polymorphism.fbld
    Structs.fbld
    Unions.fbld
    Variables.fbld
  }

  set modules_tutorial_files {
    Unit.fble
    Bit.fble
    Bit/Show.fble
    Bit4.fble
    Bit4/Show.fble
    Main.fble
  }

  # Tutorial table of contents, debug and profiling guides.
  foreach {x} {Tutorials DebugGuide ProfilingGuide} {
    ::fbld_html_doc $::b/www/tutorials/$x.html $::s/tutorials/$x.fbld
    www $::b/www/tutorials/$x.html
  }

  # Debug and Profiling Guides

  # Tutorials
  foreach {x} $tutorials {
    set base [file rootname [file tail $x]]
    ::fbld_html_tutorial $::b/www/tutorials/$base.html $::s/tutorials/$base.fbld
    www $::b/www/tutorials/$base.html
  }

  # HelloWorld tests
  fbledep $::b/tutorials/HelloWorld/HelloWorld.fble.d "/HelloWorld%" \
    "-I $::s/pkgs/core -I $::s/tutorials/HelloWorld"
  test $::b/tutorials/HelloWorld.tr \
    "$::b/pkgs/core/fble-stdio $::b/tutorials/HelloWorld/HelloWorld.fble.d" \
    "$::b/pkgs/core/fble-stdio -I $::s/pkgs/core -I $::s/tutorials/HelloWorld -m /HelloWorld%"

  # Basics tests
  fbledep $::b/tutorials/Basics/Basics.fble.d "/Basics%" \
    "-I $::s/pkgs/core -I $::s/tutorials/Basics"
  test $::b/tutorials/Basics.tr \
    "$::b/pkgs/core/fble-stdio $::b/tutorials/Basics/Basics.fble.d" \
    "$::b/pkgs/core/fble-stdio -I $::s/pkgs/core -I $::s/tutorials/Basics -m /Basics%"

  # Modules tests
  fbledep $::b/tutorials/Modules/Main.fble.d "/Main%" \
    "-I $::s/pkgs/core -I $::s/tutorials/Modules"
  test $::b/tutorials/Modules.tr \
    "$::b/pkgs/core/fble-stdio $::b/tutorials/Modules/Main.fble.d" \
    "$::b/pkgs/core/fble-stdio -I $::s/pkgs/core -I $::s/tutorials/Modules -m /Main%"
}
