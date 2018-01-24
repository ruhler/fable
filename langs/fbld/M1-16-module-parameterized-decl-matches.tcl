set prg {
  interf UnitI {
    struct Unit();
  };

  module UnitM(UnitI) {
    struct Unit();
  };

  interf IfcI {
    import @ { UnitM; };
    import UnitM { Unit; };

    func f( ; Unit);
  };

  interf MainI {
    import @ { UnitM; IfcI; };
    import UnitM { Unit; };

    func app<module I(IfcI)>( ; Unit);
    func main( ; Unit);
  };

  module MainM(MainI) {
    import @ { UnitM; IfcI; };
    import UnitM { Unit; };

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

skip fbld-test $prg "main@MainM" {} {
  return Unit@UnitM()
}
