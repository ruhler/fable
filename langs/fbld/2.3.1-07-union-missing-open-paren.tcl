set prg {
  Main.mtype {
    mtype Main {
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
      struct Unit();
      union Foo Unit x, Unit y);   # missing open paren
    };
  }
}

fbld-check-error $prg Main Main.mdefn:4:17
