set prg {
  Main.mtype {
    mtype Main<> {
      type Foo;
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      struct Unit();
      struct Foo(Unit x, Unit y Unit z);    # Missing a comma between y and Unit
    };
  }
}

fbld-check-error $prg Main Main.mdefn:4:33

