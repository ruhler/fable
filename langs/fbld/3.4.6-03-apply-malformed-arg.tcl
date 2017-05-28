set prg {
  Main.mdecl {
    mdecl Main() {
      struct Unit();
      func f(Unit x; Unit);
      func main( ; Unit);
    };
  }

  Main.mdefn {
    mdefn Main() {
      struct Unit();

      func f(Unit x; Unit) {
        x;
      };

      func main( ; Unit) {
        # The variable 'x' has not been declared.
        f(x);
      };
    };
  }
}

fbld-check-error $prg Main Main.mdefn:11:11
