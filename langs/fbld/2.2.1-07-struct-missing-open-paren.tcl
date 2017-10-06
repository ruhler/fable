set prg {
  Main.mtype {
    mtype Main {
      type Foo;
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
      struct Unit();
      struct Foo  Unit x, Unit y);    # The open paren is missing.
    };
  }
}

fbld-check-error $prg Main Main.mdefn:4:19

