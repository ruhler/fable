set prg {
  Main.mdecl {
    mdecl Main() {
      struct Unit();
      struct A(Unit x, Unit y);
      union Foo(Unit bar, A sludge);
      func main( ; Foo);
    };
  }

  Main.mdefn {
    mdefn Main() {
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
