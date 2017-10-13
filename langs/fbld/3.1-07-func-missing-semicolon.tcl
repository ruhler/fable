set prg {
  MainI.fbld {
    interf MainI {
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();

      # Missing semicolon between inputs and output.
      func f(Unit x, Unit y  Unit) {
        x;
      };
    };
  }
}
fbld-check-error $prg MainM MainM.fbld:6:30
