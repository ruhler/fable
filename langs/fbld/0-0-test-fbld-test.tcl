# Test the most basic 'fbld-test' test.
set prg {
  Main.mdecl {
    mdecl Main() {
      struct Unit();
      func main( ; Unit);
    };
  }

  Main.mdefn {
    mdefn Main() {
      struct Unit();

      func main( ; Unit) {
        Unit();
      };
    };
  }
}

fbld-test $prg Main@main {} "return Main@Unit()"
