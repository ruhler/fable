set prg {
  Main.mtype {
    mtype Main<> {
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      # A function must not be declared multiple times.
      struct Unit();

      func foo(Unit x ; Unit) {
        x;
      };

      func foo(Unit x, Unit y; Unit) {
        y;
      };
    };
  }
}
fbld-check-error $prg Main Main.mdefn:10:12
