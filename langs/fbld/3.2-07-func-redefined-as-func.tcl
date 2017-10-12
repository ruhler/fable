set prg {
  MainI.fbld {
    mtype MainI {
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      # A function must not be declared multiple times.
      struct Unit();

      func foo(Unit x ; Unit) {
        x;
      };

      func foo(Unit x, Unit y; Unit) {
        y;
      };
    };
  }
}
fbld-check-error $prg MainM MainM.fbld:10:12
