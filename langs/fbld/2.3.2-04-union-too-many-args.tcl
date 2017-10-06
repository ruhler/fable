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
        # The constructor has too many arguments.
        Foo:bar(Unit(), Unit());
      };
    };
  }
}

fbld-check-error $prg Main Main.mdefn:8:23
