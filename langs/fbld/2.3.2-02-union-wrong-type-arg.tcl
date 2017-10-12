set prg {
  MainI.fbld {
    mtype MainI {
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
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

fbld-check-error $prg MainM MainM.fbld:9:17
