set prg {
  Main.mtype {
    mtype Main {
      type Foo;
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
      struct Unit();
      struct Foo(Unit x, Unit y;    # Missing the close paren
    };
  }
}

fbld-check-error $prg Main Main.mdefn:4:32

