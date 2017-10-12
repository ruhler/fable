set prg {
  MainI.fbld {
    mtype MainI {
      type Foo;
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      struct Unit();

      # The final semicolon is missing.
      struct Foo(Unit x, Unit y)

      func foo( ; Unit) {
        Unit();
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:8:7

