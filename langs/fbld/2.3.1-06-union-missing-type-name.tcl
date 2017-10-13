set prg {
  MainI.fbld {
    interf MainI {
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();
      union (Unit x, Unit y);   # The declaration is missing a name
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:4:13
