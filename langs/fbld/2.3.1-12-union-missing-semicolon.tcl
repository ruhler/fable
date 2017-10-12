set prg {
  MainI.fbld {
    mtype MainI {
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      struct Unit();
      union Foo(Unit x, Unit y)   # missing final semicolon

      func main( ; Unit) {
        Unit();
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:6:7
