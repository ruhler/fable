set prg {
  MainI.fbld {
    mtype MainI {
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      # A function and process can't have the same name.
      struct Unit();

      func foo(Unit x ; Unit) {
        x;
      };

      proc foo( ; Unit x, Unit y; Unit) {
        $(y);
      };
    };
  }
}
fbld-check-error $prg MainM MainM.fbld:10:12
