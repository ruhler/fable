set prg {
  Main.mtype {
    mtype Main<> {
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      struct Unit();

      # Missing return type.
      func f(Unit x, Unit y; ) {
        x;
      };
    };
  }
}
fbld-check-error $prg Main Main.mdefn:6:30
