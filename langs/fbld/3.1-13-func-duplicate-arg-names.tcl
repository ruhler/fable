set prg {
  MainI.fbld {
    mtype MainI {
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      struct Unit();

      # Duplicate argument names.
      func f(Unit x, Unit x; Unit) {
        x;
      };
    };
  }
}
fbld-check-error $prg MainM MainM.fbld:6:27
