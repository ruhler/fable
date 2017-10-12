# Test that we can call a foreign function.
set prg {
  UnitI.fbld {
    mtype UnitI {
      struct Unit();
      func f( ; Unit);
    };
  }

  UnitM.fbld {
    mdefn UnitM(UnitI) {
      struct Unit();
      func f( ; Unit) {
        Unit();
      };
    };
  }

  MainI.fbld {
    mtype MainI {
      using UnitM { Unit; };
      func main( ; Unit);
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
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
