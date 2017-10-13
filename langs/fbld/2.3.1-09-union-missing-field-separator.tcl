set prg {
  MainI.fbld {
    interf MainI {
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();
      union Foo(Unit x, Unit y Unit z);   # missing field separator
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:4:32
