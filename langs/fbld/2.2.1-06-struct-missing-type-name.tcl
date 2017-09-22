set prg {
  Main.mtype {
    mtype Main<> {
      struct Unit();
      struct (Unit x, Unit y);    # The struct name is missing.
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      struct Unit();
      struct (Unit x, Unit y);    # The struct name is missing.
    };
  }
}

fbld-check-error $prg Main Main.mdefn:4:14

