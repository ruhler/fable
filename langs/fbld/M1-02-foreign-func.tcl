set prg {
  # Test that we can call a foreign function.
  interf UnitI {
    struct Unit();
    func f( ; Unit);
  };

  module UnitM(UnitI) {
    struct Unit();
    func f( ; Unit) {
      Unit();
    };
  };

  interf MainI {
    import @ { UnitM; };
    import UnitM { Unit; };
    func main( ; Unit);
  };

  module MainM(MainI) {
    import @ { UnitM; };
    import UnitM { Unit; f; };
    func main( ; Unit) {
      f();
    };
  };
}

fbld-test $prg "main@MainM" {} {
  return Unit@UnitM()
}
