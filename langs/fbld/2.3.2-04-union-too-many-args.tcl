set prg {
  MainI.fbld {
    interf MainI {
    };
  }

  MainM.fbld {
    module MainM(MainI) {
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
