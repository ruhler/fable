set prg {
  Main.mtype {
    mtype Main {
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
      struct Unit();

      # Missing entire argument from argument list.
      func f(Unit x, , Unit z; Unit) {
        x;
      };
    };
  }
}
fbld-check-error $prg Main Main.mdefn:6:22
