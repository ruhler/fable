set prg {
  Main.mtype {
    mtype Main<> {
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      struct Unit();

      # Missing function body.
      func f(Unit x, Unit y; Unit);
    };
  }
}
fbld-check-error $prg Main Main.mdefn:6:35
