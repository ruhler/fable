set prg {
  Main.mtype {
    mtype Main<> {
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      struct Unit();

      # The type Donut is not defined.
      func f(Unit x, Donut y; Unit) {
        x;
      };
    };
  }
}
fbld-check-error $prg Main Main.mdefn:6:22
