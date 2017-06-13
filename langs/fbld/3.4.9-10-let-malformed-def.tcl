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
      struct A(Unit x, Unit y);

      func main( ; A) {
        # The let argument has a syntax error.
        Unit v = ???;
        A(v, v);
      };
    };
  }
}

fbld-check-error $prg Main Main.mdefn:8:19
