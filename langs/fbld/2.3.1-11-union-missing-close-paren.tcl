set prg {
  Main.mtype {
    mtype Main {
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
      struct Unit();
      union Foo(Unit x, Unit y;   # missing close paren.
    };
  }
}

fbld-check-error $prg Main Main.mdefn:4:31
