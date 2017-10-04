set prg {
  Main.mtype {
    mtype Main<> {
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      # A struct must not be declared multiple times.
      struct Unit();
      struct A(Unit x, Unit y);
      struct A(Unit a, Unit b, Unit c);
    };
  }
}
fbld-check-error $prg Main Main.mdefn:6:14
