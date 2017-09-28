set prg {
  Main.mtype {
    mtype Main<> {
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      struct Unit();
      union Foo(Unit x, Unit y)   # missing final semicolon

      func main( ; Unit) {
        Unit();
      };
    };
  }
}

fbld-check-error $prg Main Main.mdefn:6:7
