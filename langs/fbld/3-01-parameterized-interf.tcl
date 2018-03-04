set prg {
  struct Unit();

  # Test that we can declare and use a parameterized interface.
  interf Make<type T> {
    import @ { T; };
    func make( ; T);
  };

  module MakeUnit(Make<Unit>) {
    import @ { Unit; };

    func make( ; Unit) {
      Unit();
    };
  };

  func main( ; Unit) {
    make@MakeUnit();
  };
}

fbld-test $prg "main" {} {
  return Unit()
}
