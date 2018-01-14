set prg {
  interf MainI {
    struct Unit();
    func main( ; Unit);
  };

  module MainM(MainI) {
    struct Unit();

    # Test that we can declare and use a parameterized interface.
    interf Make<type T> {
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
  };
}

fbld-test $prg "main@MainM" {} {
  return Unit@MainM()
}
