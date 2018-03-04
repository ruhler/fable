set prg {
  struct Unit();

  interf IfcI {
    import @ { Unit; };
    func f( ; Unit);
  };

  interf MainI {
    import @ { Unit; IfcI; };

    func app<module I(IfcI)>( ; Unit);
    func main( ; Unit);
  };

  module Main(MainI) {
    import @ { Unit; IfcI; };

    # Test that we can check that this module-parameterized function matches
    # its declaration in MainI.
    func app<module I(IfcI)>( ; Unit) {
      Unit();
    };

    func main( ; Unit) {
      Unit();
    };
  };
}

fbld-test $prg "main@Main" {} {
  return Unit()
}
