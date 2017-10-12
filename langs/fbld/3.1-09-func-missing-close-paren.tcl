set prg {
  MainI.fbld {
    mtype MainI {
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      struct Unit();

      # Missing close paren
      func f(Unit x, Unit y; Unit {
        x;
      };
    };
  }
}
fbld-check-error $prg MainM MainM.fbld:6:35
