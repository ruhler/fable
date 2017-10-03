set prg {
  Main.mtype {
    mtype Main<> {
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      struct Unit();

      # The return type Donut is not defined.
      func f(Unit x; Donut) {
        Donut();
      };
    };
  }
}
fbld-check-error $prg Main Main.mdefn:6:22
