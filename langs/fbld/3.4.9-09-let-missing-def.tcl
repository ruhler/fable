set prg {
  Main.mtype {
    mtype Main<> {
      struct Unit();
      struct A(Unit x, Unit y);
      func main( ; A);
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      struct Unit();
      struct A(Unit x, Unit y);

      func main( ; A) {
        # The body of the let is missing.
        Unit v = Unit();
      };
    };
  }
}

fbld-check-error $prg Main Main.mdefn:9:7
