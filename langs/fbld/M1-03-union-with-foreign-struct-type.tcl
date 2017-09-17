set prg {
  Types.mtype {
    mtype Types<> {
      struct Unit();
      struct Foo(Unit bar);
    };
  }

  Types.mdefn {
    mdefn Types< ; ; Types<>> {
      struct Unit();
      struct Foo(Unit bar);
    };
  }

  Main.mtype {
    mtype Main<> {
      using Types<;> { Foo; };
      func main( ; Foo);
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      using Types<;> { Unit; Foo; };
      func main( ; Foo) {
        # Foo is a struct type, not a union type.
        Foo:bar(Unit());
      };
    };
  }
}

skip fbld-check-error $prg Main Main.mdefn:6:9
