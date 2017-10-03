set prg {
  Main.mtype {
    mtype Main<> {
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      struct Unit();

      # Function declarations must have an open parenthesis.
      func f Unit x, Unit y; Unit) {
        x;
      };
    };
  }
}
fbld-check-error $prg Main Main.mdefn:6:14
