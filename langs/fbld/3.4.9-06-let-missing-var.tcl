set prg {
  Main.mtype {
    mtype Main {
      struct Unit();
      struct A(Unit x, Unit y);
      func main( ; A);
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
      struct Unit();
      struct A(Unit x, Unit y);

      func main( ; A) {
        # The variable name is missing.
        Unit = Unit();
        A(v, v);
      };
    };
  }
}

fbld-check-error $prg Main Main.mdefn:8:14
