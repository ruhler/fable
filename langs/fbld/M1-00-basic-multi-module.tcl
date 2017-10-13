# A basic test case using multiple modules.
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
      func main( ; Unit@UnitM);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      func main( ; Unit@UnitM) {
        Unit@UnitM();
      };
    };
  }
}

fbld-test $prg "main@MainM" {} {
  return Unit@UnitM()
}
