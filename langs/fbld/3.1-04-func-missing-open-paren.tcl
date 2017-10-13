set prg {
  MainI.fbld {
    interf MainI {
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();

      # Function declarations must have an open parenthesis.
      func f Unit x, Unit y; Unit) {
        x;
      };
    };
  }
}
fbld-check-error $prg MainM MainM.fbld:6:14
