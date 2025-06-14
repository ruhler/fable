namespace eval "tutorials" {
  set tutorials {
    Basics.fbld
    Bind.fbld
    Core.fbld
    Features.fbld
    Functions.fbld
    HelloWorld.fbld
    Install.fbld
    Introduction.fbld
    Lists.fbld
    Literals.fbld
    Modules.fbld
    Polymorphism.fbld
    PrivateTypes.fbld
    Structs.fbld
    Stdio.fbld
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
    www tutorials/$x.html
  }

  # Debug and Profiling Guides

  # Tutorials
  foreach {x} $tutorials {
    set base [file rootname [file tail $x]]
    ::fbld_html_tutorial $::b/www/tutorials/$base.html $::s/tutorials/$base.fbld
    www tutorials/$base.html
  }

  # HelloWorld tests
  run_stdio $::b/tutorials/HelloWorld.tr.out \
    "-I $::s/pkgs/core -I $::s/tutorials/HelloWorld -m /HelloWorld%"
  test $::b/tutorials/HelloWorld.tr $::b/tutorials/HelloWorld.tr.out \
    "cat $::b/tutorials/HelloWorld.tr.out"

  # Basics tests
  run_stdio $::b/tutorials/Basics.tr.out \
    "-I $::s/pkgs/core -I $::s/tutorials/Basics -m /Basics%"
  test $::b/tutorials/Basics.tr $::b/tutorials/Basics.tr.out \
    "cat $::b/tutorials/Basics.tr.out"

  # Modules tests
  run_stdio $::b/tutorials/Modules.tr.out \
    "-I $::s/pkgs/core -I $::s/pkgs/core -I $::s/tutorials/Modules -m /Main%"
  test $::b/tutorials/Modules.tr $::b/tutorials/Modules.tr.out \
    "cat $::b/tutorials/Modules.tr.out"
}
