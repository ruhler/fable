set prg {
  Main.mtype {
    mtype Main {
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
      struct Unit();
      union Foo(Unit bar);

      func main( ; Foo) {
        # The constructor is missing its argument.
        Foo:bar();
      };
    };
  }
}

fbld-check-error $prg Main Main.mdefn:8:17
