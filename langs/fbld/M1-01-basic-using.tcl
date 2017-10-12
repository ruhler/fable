# Test that we can use a 'using' statement to refer to foreign entities.
set prg {
  UnitI.fbld {
    mtype UnitI {
      struct Unit();
    };
  }

  UnitM.fbld {
    mdefn UnitM(UnitI) {
      struct Unit();
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
      using UnitM { Unit; };
      func main( ; Unit) {
        Unit();
      };
    };
  }
}

fbld-test $prg "main@MainM" {} {
  return Unit@UnitM()
}
