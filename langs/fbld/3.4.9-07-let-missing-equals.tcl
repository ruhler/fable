set prg {
  Main.mtype {
    mtype Main> {
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
        # The equals sign is missing.
        Unit v Unit();
        A(v, v);
      };
    };
  }
}

fbld-check-error $prg Main Main.mdefn:8:16
