set prg {
  Main.mdecl {
    mdecl Main() {
      struct Unit();
      struct A(Unit x, Unit y);
      func main( ; Unit);
    };
  }

  Main.mdefn {
    mdefn Main() {
      struct Unit();
      struct A(Unit x, Unit y);

      func main( ; Unit) {
        # The variable 'x' has not been declared.
        x.y;
      };
    };
  }
}
fbld-check-error $prg Main Main.mdefn:8:9
