set prg {
  Main.mdecl {
    mdecl Main() {
      struct Unit();
      struct A(Unit x, Unit y);
      func main( ; A);
    };
  }

  Main.mdefn {
    mdefn Main() {
      struct Unit();

      # The declaration of 'A' here doesn't match the declaration in
      # Main.mdecl.
      struct A(Unit x);

      func main( ; A) {
        A(Unit());
      };
    };
  }
}

skip fbld-check-error $prg Main Main.mdefn:7:14
