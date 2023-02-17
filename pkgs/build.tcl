namespace eval "pkgs" {
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

        # Generate a .d file to capture dependencies.
        build $::b/pkgs/$name/$x.d "$::b/bin/fble-deps $::s/pkgs/$name/$x" \
          "$::b/bin/fble-deps $cflags -t $::b/pkgs/$name/$x.d -m $mpath > $::b/pkgs/$name/$x.d" \
          "depfile = $::b/pkgs/$name/$x.d"

        fbleobj $::b/pkgs/$name/$x.o $::b/bin/fble-compile "-c $cflags -m $mpath" $::b/pkgs/$name/$x.d
        lappend objs $::b/pkgs/$name/$x.o

        set target $::config::datadir/fble/$name/$x
        install $::s/pkgs/$name/$x $target
      }
    }

    lib $::b/pkgs/$name/libfble-$name.a $objs
  }

  build_tcl $::s/pkgs/core/build.tcl
  build_tcl $::s/pkgs/app/build.tcl
  build_tcl $::s/pkgs/md5/build.tcl
  build_tcl $::s/pkgs/sat/build.tcl
  build_tcl $::s/pkgs/hwdg/build.tcl
  build_tcl $::s/pkgs/games/build.tcl
  build_tcl $::s/pkgs/invaders/build.tcl
  build_tcl $::s/pkgs/pinball/build.tcl
  build_tcl $::s/pkgs/graphics/build.tcl
}
