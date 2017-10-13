set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      struct (Unit x, Unit y);    # The struct name is missing.
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();
      struct (Unit x, Unit y);    # The struct name is missing.
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:4:14

