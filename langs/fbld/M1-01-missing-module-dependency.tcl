set prg {
  Unit.mdecl {
    mdecl Unit() {
      struct Unit();
    };
  }

  Unit.mdefn {
    mdefn Unit() {
      struct Unit();
    };
  }

  Main.mdecl {
    mdecl Main(Unit) {
      func Main( ; Unit@Unit);
    };
  }

  Main.mdefn {
    # The Main module must list Unit as a dependency.
    mdefn Main() {
      func main( ; Unit@Unit) {
        Unit@Unit();
      };
    };
  }
}

skip fbld-check-error $prg Main@main
