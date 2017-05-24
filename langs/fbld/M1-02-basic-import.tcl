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
    mdefn Main(Unit) {
      import Unit(Unit);

      func main( ; Unit) {
        Unit();
      };
    };
  }
}

fbld-test $prg Main@main {} "return Unit@Unit()"
