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
        # The constructor has too many arguments.
        Foo:bar(Unit(), Unit());
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:8:23
