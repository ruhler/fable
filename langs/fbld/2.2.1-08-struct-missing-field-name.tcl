set prg {
  MainI.fbld {
    mtype MainI {
      type Foo;
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      struct Unit();
      struct Foo(Unit x, Unit, Unit z);    # The second field is missing a name
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:4:30

