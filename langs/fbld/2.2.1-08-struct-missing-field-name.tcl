set prg {
  Main.mtype {
    mtype Main<> {
      type Foo;
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      struct Unit();
      struct Foo(Unit x, Unit, Unit z);    # The second field is missing a name
    };
  }
}

fbld-check-error $prg Main Main.mdefn:4:30

