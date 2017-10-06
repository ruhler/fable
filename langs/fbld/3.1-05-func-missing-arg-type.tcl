set prg {
  Main.mtype {
    mtype Main {
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
      struct Unit();

      # Arg missing type name.
      func f(x, Unit y; Unit) {
        x;
      };
    };
  }
}
fbld-check-error $prg Main Main.mdefn:6:15
