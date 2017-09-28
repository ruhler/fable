set prg {
  Main.mtype {
    mtype Main<> {
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      struct Unit();
      union Foo(Unit x, Unit y Unit z);   # missing field separator
    };
  }
}

fbld-check-error $prg Main Main.mdefn:4:32
