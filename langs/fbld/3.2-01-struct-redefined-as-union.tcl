set prg {
  MainI.fbld {
    mtype MainI {
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      # A union and a struct can't have the same name.
      struct Unit();
      struct A(Unit x, Unit y);
      union A(Unit a, Unit b, Unit c);
    };
  }
}
fbld-check-error $prg MainM MainM.fbld:6:13
