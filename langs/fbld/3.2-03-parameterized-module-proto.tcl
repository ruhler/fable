set prg {
  struct Unit();

  interf Make<type T> {
    import @ { T; };
    func make( ; T);
  };

  # Test that we can declare and use a parameterized module parameter.
  func foo<type A, module M(Make<A>)>( ; A) {
    make@M();
  };

  module MakeUnit(Make<Unit>) {
    import @ { Unit; };

    func make( ; Unit) {
      Unit();
    };
  };

  func main( ; Unit) {
    foo<Unit, MakeUnit>();
  };
}

fbld-test $prg "main" {} {
  return Unit()
}
