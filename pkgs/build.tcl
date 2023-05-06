namespace eval "pkgs" {
  set pkgs {
    core
    app
    fbld
    md5
    sat
    hwdg
    games
    invaders
    pinball
    graphics
  }

  # pkg --
  #   Build an fble library package.
  #
  # Inputs:
  #   name - the name of the package, such as 'app'.
  #   deps - list of fble packages (without fble- prefix) this package depends
  #          on.
  #   objs - additional object files to include in the generated library.
  proc ::pkg {name deps objs} {
    set cflags "-I $::s/pkgs/$name"
    foreach dep $deps {
      append cflags " -I $::s/pkgs/$dep"
    }

    foreach dir [dirs $::s/pkgs/$name ""] {
      foreach {y} [build_glob $::s/pkgs/$name/$dir -tails -nocomplain -type f *.fble] {
        set x $dir$y
        set mpath "/[file rootname $x]%"

        set target $::config::datadir/fble/$name/$x
        install $::s/pkgs/$name/$x $target

        fbledep $::b/pkgs/$name/$x.d $mpath $cflags
        fbleobj $::b/pkgs/$name/$x.o $::b/bin/fble-compile "-c $cflags -m $mpath" $::b/pkgs/$name/$x.d
        lappend objs $::b/pkgs/$name/$x.o
      }
    }

    lib $::b/pkgs/$name/libfble-$name.a $objs
    install $::b/pkgs/$name/libfble-$name.a $::config::libdir/libfble-$name.a
  }

  foreach {x} $pkgs {
    build_tcl $::s/pkgs/$x/build.tcl
  }
}
