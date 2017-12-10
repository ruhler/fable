set prg {
  UnitI.fbld {
    interf UnitI {
      struct Unit();
    };
  }

  UnitM.fbld {
    module UnitM(UnitI) {
      struct Unit();
    };
  }

  IfcI.fbld {
    interf IfcI {
      import @ { UnitM; };
      import UnitM { Unit; };

      func f( ; Unit);
    };
  }

  MainI.fbld {
    interf MainI {
      import @ { UnitM; IfcI; };
      import UnitM { Unit; };

      func app< ; IfcI I>( ; Unit);
      func main( ; Unit);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      import @ { UnitM; IfcI; };
      import UnitM { Unit; };

      # Test that we can check that this module-parameterized function matches
      # its declaration in MainI.
      func app< ; IfcI I>( ; Unit) {
        Unit();
      };

      func main( ; Unit) {
        Unit();
      };
    };
  }
}

fbld-test $prg "main@MainM" {} {
  return Unit@UnitM()
}
