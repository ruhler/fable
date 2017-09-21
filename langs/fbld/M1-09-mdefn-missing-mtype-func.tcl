set prg {
  Main.mtype {
    mtype Main<> {
      struct Unit();
      func f(Unit x ; Unit);
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      # The function f is not declared locally.
      struct Unit();
    };
  }
}

fbld-check-error $prg Main Main.mdefn:2:11
