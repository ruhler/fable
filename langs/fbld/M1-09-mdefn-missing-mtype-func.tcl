set prg {
  MainI.fbld {
    mtype MainI {
      struct Unit();
      func f(Unit x ; Unit);
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      # The function f is not declared locally.
      struct Unit();
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:2:11
