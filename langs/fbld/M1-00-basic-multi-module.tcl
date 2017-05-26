# A basic test case using multiple modules.
set prg {
  UnitM.mdecl {
    mdecl UnitM() {
      struct Unit();
    };
  }

  UnitM.mdefn {
    mdefn UnitM() {
      struct Unit();
    };
  }

  Main.mdecl {
    mdecl Main(UnitM) {
      func Main( ; Unit@UnitM);
    };
  }

  Main.mdefn {
    mdefn Main(UnitM) {
      func main( ; Unit@UnitM) {
        Unit@UnitM();
      };
    };
  }
}

fbld-test $prg main@Main {} "return Unit@UnitM()"
