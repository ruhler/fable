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
        # No field is provided for the constructor.
        Foo:(Unit());
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:9:13
