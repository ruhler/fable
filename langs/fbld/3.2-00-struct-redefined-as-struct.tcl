set prg {
  MainI.fbld {
    mtype MainI {
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      # A struct must not be declared multiple times.
      struct Unit();
      struct A(Unit x, Unit y);
      struct A(Unit a, Unit b, Unit c);
    };
  }
}
fbld-check-error $prg MainM MainM.fbld:6:14
