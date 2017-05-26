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
    # The Main module must list Unit as a dependency.
    mdefn Main() {
      func main( ; Unit@UnitM) {
        Unit@UnitM();
      };
    };
  }
}

skip fbld-check-error $prg main@Main
