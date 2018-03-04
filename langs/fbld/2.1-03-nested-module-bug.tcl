set prg {
  interf IdI<type T> {
    import @ { T; };
    func id(T a ; T);
  };

  interf UnitI {
    import @ { IdI; };

    struct Unit();
    module Id(IdI<Unit>);
  };

  module UnitM(UnitI) {
    import @ { IdI; };
    
    struct Unit();
    module Id(IdI<Unit>) {
      import @ { Unit; };

      func id(Unit a; Unit) a;
    };
  };

  interf MainI {
    import @ { UnitM; IdI; };
    import UnitM { Unit; };

    struct Foo(Unit a);
    module Id(IdI<Foo>);

    func main( ; Unit);
  };

  module MainM(MainI) {
    import @ { UnitM; IdI; };
    import UnitM { Unit; };

    struct Foo(Unit a);
    
    module Id(IdI<Foo>) {
      import @ { UnitM; Foo; };

      # There was a bug in the past where this would give an error on the
      # following line, column 43: "error: Expected type Unit, but found
      # type Unit@UnitM"
      func id(Foo a; Foo) Foo(id@Id@UnitM(a.a));
    };

    func main( ; Unit) {
      id@Id(Foo(Unit())).a;
    };
  };
}

fbld-test $prg "main@MainM" {} {
  return Unit@UnitM()
}
