set prg {
  MainI.fbld {
    mtype MainI {
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      # A union must not be declared multiple times.
      struct Unit();
      union A(Unit x, Unit y);
      union A(Unit a, Unit b, Unit c);
    };
  }
}
fbld-check-error $prg MainM MainM.fbld:6:13
