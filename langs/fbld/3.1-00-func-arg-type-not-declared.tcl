set prg {
  MainI.fbld {
    mtype MainI {
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      struct Unit();

      # The type Donut is not defined.
      func f(Unit x, Donut y; Unit) {
        x;
      };
    };
  }
}
fbld-check-error $prg MainM MainM.fbld:6:22
