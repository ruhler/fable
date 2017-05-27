set prg {
  Main.mdecl {
    mdecl Main() {
      struct Unit();
      func f(Unit x, Unit y; Unit);
      func main( ; Unit);
    };
  }

  Main.mdefn {
    mdefn Main() {
      # Function declarations must have a final semicolon.
      struct Unit();

      func f(Unit x, Unit y; Unit) {
        x;
      }

      func main( ; Unit) {
        Unit();
      };
    };
  }
}
fbld-check-error $prg Main Main.mdefn:10:7
