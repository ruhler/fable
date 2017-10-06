set prg {
  Main.mtype {
    mtype Main {
      struct Unit();
      func f(Unit x, Unit y; Unit);
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
      struct Unit();

      # The declaration of 'f' here doesn't match the declaration in
      # Main.mtype.
      func f(Unit x ; Unit) {
        Unit();
      };
    };
  }
}

fbld-check-error $prg Main Main.mdefn:7:12
