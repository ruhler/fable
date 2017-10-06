set prg {
  Main.mtype {
    mtype Main {
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
      struct Unit();
      struct A(Unit x, Unit y);
      union Foo(Unit bar, A sludge);

      func main( ; Foo) {
        # The open parenthesis before the argument is missing.
        Foo:bar ?(Foo:bar(Unit()) ; Unit(), Unit()));
      };
    };
  }
}

fbld-check-error $prg Main Main.mdefn:9:17
