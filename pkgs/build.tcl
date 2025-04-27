namespace eval "pkgs" {
  set pkgs {
    core
    app
    network
    fbld
    md5
    pprof
    pprof2
    sat
    satgame
    hwdg
    games
    invaders
    pinball
    graphics
    benchmark
  }

  # pkg --
  #   Build an fble library package.
  #
  # Inputs:
  #   name - the name of the package, such as 'app'.
  #   deps - list of fble packages (without fble- prefix) this package depends
  #          on.
  #   gens - A list of generated .fble files to include in the package.
  #   objs - additional object files to include in the generated library.
  proc ::pkg {name deps gens objs} {
    set cflags "-I $::s/pkgs/$name -I $::b/pkgs/$name"
    foreach dep $deps {
      append cflags " -I $::s/pkgs/$dep"
      append cflags " -I $::b/pkgs/$dep"
    }

    # Automatically include all .fble files under the $::s/pkgs/$name
    # directory.
    set root $::s/pkgs/$name
    foreach dir [dirs $root ""] {
      foreach {y} [build_glob $root/$dir -tails -nocomplain -type f *.fble] {
        set x $dir$y
        set mpath "/[file rootname $x]%"

        set target $::config::datadir/fble/$name/$x
        install $root/$x $target

        fbleobj $::b/pkgs/$name/$x.o $::b/bin/fble-compile "-c $cflags -m $mpath" $gens
        lappend objs $::b/pkgs/$name/$x.o
      }
    }

    # Install all .fble.2 files under the $::s/pkgs/$name directory.
    set root $::s/pkgs/$name
    foreach dir [dirs $root ""] {
      foreach {y} [build_glob $root/$dir -tails -nocomplain -type f *.fble.@] {
        set x $dir$y
        set target $::config::datadir/fble/$name/$x
        install $root/$x $target
      }
    }

    # Explicitly requested build-generated .fble files under the
    # $::b/pkgs/$name directory.
    foreach gen $gens {
      set x [string range $gen [string length $::b/pkgs/$name/] end]
      set mpath "/[file rootname $x]%"

      set target $::config::datadir/fble/$name/$x
      install $gen $target

      fbleobj $::b/pkgs/$name/$x.o $::b/bin/fble-compile "-c $cflags -m $mpath" $gens
      lappend objs $::b/pkgs/$name/$x.o
    }

    lib $::b/pkgs/$name/libfble-$name$::lext $objs
    install $::b/pkgs/$name/libfble-$name$::lext $::config::libdir/libfble-$name$::lext
  }

  foreach {x} $pkgs {
    build_tcl $::s/pkgs/$x/build.tcl
  }
}
