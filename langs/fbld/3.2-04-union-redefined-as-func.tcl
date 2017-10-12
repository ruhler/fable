set prg {
  MainI.fbld {
    mtype MainI {
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      # A union and a function can't have the same name.
      struct Unit();
      union A(Unit x, Unit y);

      func A(Unit a, Unit b, Unit c ; Unit) {
        Unit();
      };
    };
  }
}
fbld-check-error $prg MainM MainM.fbld:7:12
