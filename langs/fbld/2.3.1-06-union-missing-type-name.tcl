set prg {
  Main.mtype {
    mtype Main {
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
      struct Unit();
      union (Unit x, Unit y);   # The declaration is missing a name
    };
  }
}

fbld-check-error $prg Main Main.mdefn:4:13
