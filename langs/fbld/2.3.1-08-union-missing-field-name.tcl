set prg {
  Main.mtype {
    mtype Main {
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
      struct Unit();
      union Foo(Unit x, Unit, Unit y);   # missing field name
    };
  }
}

fbld-check-error $prg Main Main.mdefn:4:29
