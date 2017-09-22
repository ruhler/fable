set prg {
  Main.mtype {
    mtype Main<> {
      type Foo;
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      struct Unit();

      # The final semicolon is missing.
      struct Foo(Unit x, Unit y)

      func foo( ; Unit) {
        Unit();
      };
    };
  }
}

fbld-check-error $prg Main Main.mdefn:8:7

