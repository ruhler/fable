# A basic test case using multiple modules.
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
    mdecl Main() {
      func Main( ; Unit@Unit);
    };
  }

  Main.mdefn {
    mdefn Main() {
      func main( ; Unit@Unit) {
        Unit@Unit();
      };
    };
  }
}

skip fbld-test $prg Main@main {} "return Unit@Unit()"
