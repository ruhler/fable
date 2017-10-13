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
        # The constructor is missing its argument.
        Foo:bar();
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:8:17
