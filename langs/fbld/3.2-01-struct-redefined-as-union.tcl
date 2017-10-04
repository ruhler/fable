set prg {
  Main.mtype {
    mtype Main<> {
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      # A union and a struct can't have the same name.
      struct Unit();
      struct A(Unit x, Unit y);
      union A(Unit a, Unit b, Unit c);
    };
  }
}
fbld-check-error $prg Main Main.mdefn:6:13
