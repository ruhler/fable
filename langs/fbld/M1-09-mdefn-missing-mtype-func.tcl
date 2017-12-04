set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      func f(Unit x ; Unit);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      # The function f is not declared locally.
      struct Unit();
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:2:12
