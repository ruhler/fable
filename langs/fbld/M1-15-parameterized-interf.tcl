set prg {
  interf MainI {
    struct Unit();
    func main( ; Unit);
  };

  module MainM(MainI) {
    struct Unit();

    # Test that we can declare and use a parameterized interface.
    priv interf Make<type T> {
      import @ { T; };
      func make( ; T);
    };

    priv module MakeUnit(Make<Unit>) {
      import @ { Unit; };

      func make( ; Unit) {
        Unit();
      };
    };

    func main( ; Unit) {
      make@MakeUnit();
    };
  };
}

fbld-test $prg "main@MainM" {} {
  return Unit@MainM()
}
