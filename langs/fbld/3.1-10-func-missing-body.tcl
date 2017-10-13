set prg {
  MainI.fbld {
    interf MainI {
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();

      # Missing function body.
      func f(Unit x, Unit y; Unit);
    };
  }
}
fbld-check-error $prg MainM MainM.fbld:6:35
