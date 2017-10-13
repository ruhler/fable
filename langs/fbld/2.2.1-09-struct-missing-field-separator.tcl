set prg {
  MainI.fbld {
    interf MainI {
      type Foo;
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();
      struct Foo(Unit x, Unit y Unit z);    # Missing a comma between y and Unit
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:4:33

