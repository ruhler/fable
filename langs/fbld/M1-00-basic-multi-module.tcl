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
      import @ { UnitM; };
      func main( ; Unit@UnitM);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      import @ { UnitM; };
      func main( ; Unit@UnitM) {
        Unit@UnitM();
      };
    };
  }
}

fbld-test $prg "main@MainM" {} {
  return Unit@UnitM()
}
