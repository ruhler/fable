set prg {
  module Types {
    struct Unit();
    struct Foo(Unit bar);
  };

  module M {
    import @ { Types; };

    func main( ; Foo@Types) {
      # Foo is a struct type, not a union type.
      Foo@Types:bar(Unit@Types());
    };
  };
}

fbld-check-error $prg 12:7
