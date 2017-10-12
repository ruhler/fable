set prg {
  MainI.fbld {
    mtype MainI {
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      struct Unit();
      union Foo(Unit bar);

      func main( ; Foo) {
        # The constructor is missing its argument.
        Foo:bar();
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:8:17
