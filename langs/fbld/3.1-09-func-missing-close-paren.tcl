set prg {
  Main.mtype {
    mtype Main<> {
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      struct Unit();

      # Missing close paren
      func f(Unit x, Unit y; Unit {
        x;
      };
    };
  }
}
fbld-check-error $prg Main Main.mdefn:6:35
