set prg {
  MainI.fbld {
    mtype MainI {
      type Foo;
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      struct Unit();
      struct Foo(Unit x, Unit y Unit z);    # Missing a comma between y and Unit
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:4:33

