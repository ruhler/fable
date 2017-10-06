set prg {
  Main.mtype {
    mtype Main {
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
      # A function and process can't have the same name.
      struct Unit();

      func foo(Unit x ; Unit) {
        x;
      };

      proc foo( ; Unit x, Unit y; Unit) {
        $(y);
      };
    };
  }
}
fbld-check-error $prg Main Main.mdefn:10:12
