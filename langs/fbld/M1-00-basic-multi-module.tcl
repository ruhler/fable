# A basic test case using multiple modules.
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
      func main( ; Unit@UnitM);
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      func main( ; Unit@UnitM) {
        Unit@UnitM();
      };
    };
  }
}

fbld-test $prg "main@MainM" {} {
  return Unit@UnitM()
}
