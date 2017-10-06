set prg {
  Main.mtype {
    mtype Main {
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
      # A union and a function can't have the same name.
      struct Unit();
      union A(Unit x, Unit y);

      func A(Unit a, Unit b, Unit c ; Unit) {
        Unit();
      };
    };
  }
}
fbld-check-error $prg Main Main.mdefn:7:12
