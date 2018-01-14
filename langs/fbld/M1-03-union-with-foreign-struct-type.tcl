set prg {
  interf TypesI {
    struct Unit();
    struct Foo(Unit bar);
  };

  module TypesM(TypesI) {
    struct Unit();
    struct Foo(Unit bar);
  };

  interf MainI {
    import @ { TypesM; };
    import TypesM { Foo; };
    func main( ; Foo);
  };

  module MainM(MainI) {
    import @ { TypesM; };
    import TypesM { Unit; Foo; };
    func main( ; Foo) {
      # Foo is a struct type, not a union type.
      Foo:bar(Unit());
    };
  };
}

fbld-check-error $prg 23:7
