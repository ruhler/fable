set prg {
  TypesI.fbld {
    mtype TypeI {
      struct Unit();
      struct Foo(Unit bar);
    };
  }

  TypesM.fbld {
    mdefn TypesM(TypesI) {
      struct Unit();
      struct Foo(Unit bar);
    };
  }

  MainI.fbld {
    mtype MainI {
      using TypesM { Foo; };
      func main( ; Foo);
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      using TypesM { Unit; Foo; };
      func main( ; Foo) {
        # Foo is a struct type, not a union type.
        Foo:bar(Unit());
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:6:9
