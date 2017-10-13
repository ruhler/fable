set prg {
  MainI.fbld {
    interf MainI {
      type Foo;
    };
  }

  MainM.fbld {
    module MainM(MainI) {
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

