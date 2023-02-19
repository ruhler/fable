namespace eval "tutorials" {
  set tutorials {
    AbstractTypes.fbld
    Basics.fbld
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

  dist_s $::s/tutorials/build.tcl
  dist_s $::s/tutorials/Tutorials.fbld
  dist_s $::s/tutorials/debug_guide.txt
  dist_s $::s/tutorials/profile_guide.txt
  dist_s $::s/tutorials/Basics/Basics.fble
  dist_s $::s/tutorials/HelloWorld/HelloWorld.fble
  foreach {x} $tutorials { dist_s $::s/tutorials/$x }
  foreach {x} $modules_tutorial_files { dist_s $::s/tutorials/Modules/$x }

  # Tutorial table of contents
  ::html_doc $::d/www/tutorials/Tutorials.html $::s/tutorials/Tutorials.fbld
  www $::d/www/tutorials/Tutorials.html
  dist_d $::d/www/tutorials/Tutorials.html

  # Tutorials
  foreach {x} $tutorials {
    set base [file rootname [file tail $x]]
    ::html_tutorial $::d/www/tutorials/$base.html $::s/tutorials/$base.fbld
    www $::d/www/tutorials/$base.html
    dist_d $::d/www/tutorials/$base.html
  }

  # HelloWorld tests
  test $::b/tutorials/HelloWorld.tr \
    "$::b/pkgs/core/fble-stdio $::s/tutorials/HelloWorld/HelloWorld.fble" \
    "$::b/pkgs/core/fble-stdio -I $::s/pkgs/core -I $::s/tutorials/HelloWorld -m /HelloWorld%"

  # Basics tests
  test $::b/tutorials/Basics.tr \
    "$::b/pkgs/core/fble-stdio $::s/tutorials/Basics/Basics.fble" \
    "$::b/pkgs/core/fble-stdio -I $::s/pkgs/core -I $::s/tutorials/Basics -m /Basics%"

  # Modules tests
  set modules_deps [list $::b/pkgs/core/fble-stdio]
  foreach {x} $modules_tutorial_files {
    lappend modules_deps $::s/tutorials/Modules/$x
  }
  test $::b/tutorials/Modules.tr $modules_deps \
    "$::b/pkgs/core/fble-stdio -I $::s/pkgs/core -I $::s/tutorials/Modules -m /Main%"
}
