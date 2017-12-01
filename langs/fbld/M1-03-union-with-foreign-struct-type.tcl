set prg {
  TypesI.fbld {
    interf TypesI {
      struct Unit();
      struct Foo(Unit bar);
    };
  }

  TypesM.fbld {
    module TypesM(TypesI) {
      struct Unit();
      struct Foo(Unit bar);
    };
  }

  MainI.fbld {
    interf MainI {
      import @ { TypesM; };
      import TypesM { Foo; };
      func main( ; Foo);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      import @ { TypesM; };
      import TypesM { Unit; Foo; };
      func main( ; Foo) {
        # Foo is a struct type, not a union type.
        Foo:bar(Unit());
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:7:9
