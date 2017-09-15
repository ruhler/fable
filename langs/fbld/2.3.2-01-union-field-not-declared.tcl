set prg {
  Main.mtype {
    mtype Main<> {
      struct Unit();
      struct A(Unit x, Unit y);
      union Foo(Unit bar, A sludge);
      func main( ; Foo);
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      struct Unit();
      struct A(Unit x, Unit y);
      union Foo(Unit bar, A sludge);

      func main( ; Foo) {
        # Foo has no field called "blah".
        Foo:blah(Unit());
      };
    };
  }
}

fbld-check-error $prg Main Main.mdefn:9:13
