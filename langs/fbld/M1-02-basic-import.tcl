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
      import UnitM(Unit);

      func main( ; Unit) {
        Unit();
      };
    };
  }
}

fbld-test $prg main@Main {} "return Unit@UnitM()"
