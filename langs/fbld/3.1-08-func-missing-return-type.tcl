set prg {
  MainI.fbld {
    mtype MainI {
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      struct Unit();

      # Missing return type.
      func f(Unit x, Unit y; ) {
        x;
      };
    };
  }
}
fbld-check-error $prg MainM MainM.fbld:6:30
