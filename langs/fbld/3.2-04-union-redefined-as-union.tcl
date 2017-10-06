set prg {
  Main.mtype {
    mtype Main {
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
      # A union must not be declared multiple times.
      struct Unit();
      union A(Unit x, Unit y);
      union A(Unit a, Unit b, Unit c);
    };
  }
}
fbld-check-error $prg Main Main.mdefn:6:13
