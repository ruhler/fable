# Test that we can call a foreign function.
set prg {
  UnitI.fbld {
    interf UnitI {
      struct Unit();
      func f( ; Unit);
    };
  }

  UnitM.fbld {
    module UnitM(UnitI) {
      struct Unit();
      func f( ; Unit) {
        Unit();
      };
    };
  }

  MainI.fbld {
    interf MainI {
      using UnitM { Unit; };
      func main( ; Unit);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      using UnitM { Unit; f; };
      func main( ; Unit) {
        f();
      };
    };
  }
}

fbld-test $prg "main@MainM" {} {
  return Unit@UnitM()
}
