set prg {
  Main.mtype {
    mtype Main<> {
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      struct Unit();

      # Duplicate argument names.
      func f(Unit x, Unit x; Unit) {
        x;
      };
    };
  }
}
fbld-check-error $prg Main Main.mdefn:6:27
