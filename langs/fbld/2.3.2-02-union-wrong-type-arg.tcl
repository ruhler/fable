set prg {
  Main.mtype {
    mtype Main<> {
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      struct Unit();
      struct A(Unit x, Unit y);
      union Foo(Unit bar, A sludge);

      func main( ; Foo) {
        # The bar field should have type Unit, not A.
        Foo:bar(A(Unit(), Unit()));
      };
    };
  }
}

fbld-check-error $prg Main Main.mdefn:9:17
