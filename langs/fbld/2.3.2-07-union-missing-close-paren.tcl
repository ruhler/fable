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
        # The close paren is missing.
        Foo:bar(Unit();
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:9:23
