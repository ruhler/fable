set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      func f(Unit x, Unit y; Unit);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();

      # The declaration of 'f' here doesn't match the declaration in
      # Main.interf.
      func f(Unit x ; Unit) {
        Unit();
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:7:12
