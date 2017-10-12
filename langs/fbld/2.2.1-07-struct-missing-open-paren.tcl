set prg {
  MainI.fbld {
    mtype MainI {
      type Foo;
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      struct Unit();
      struct Foo  Unit x, Unit y);    # The open paren is missing.
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:4:19

