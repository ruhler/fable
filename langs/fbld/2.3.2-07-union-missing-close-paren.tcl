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
        # The close paren is missing.
        Foo:bar(Unit();
      };
    };
  }
}

fbld-check-error $prg Main Main.mdefn:9:23
