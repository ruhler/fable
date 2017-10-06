set prg {
  Main.mtype {
    mtype Main {
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
      # Declared functions must have a name.
      struct Unit();

      func (Unit x, Unit y; Unit) {
        x;
      };
    };
  }
}
fbld-check-error $prg Main Main.mdefn:6:12
