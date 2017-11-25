# Test that we can use a 'using' statement to refer to foreign entities.
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

  MainI.fbld {
    interf MainI {
      import @ { UnitM; };
      import UnitM { Unit; };
      func main( ; Unit);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      import @ { UnitM; };
      import UnitM { Unit; };
      func main( ; Unit) {
        Unit();
      };
    };
  }
}

fbld-test $prg "main@MainM" {} {
  return Unit@UnitM()
}
