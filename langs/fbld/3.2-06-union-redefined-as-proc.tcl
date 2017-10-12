set prg {
  MainI.fbld {
    mtype MainI {
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      # A union and a process can't have the same name.
      struct Unit();
      union A(Unit x, Unit y);

      proc A( ; Unit a, Unit b ; Unit) {
        $(Unit());
      };
    };
  }
}
fbld-check-error $prg MainM MainM.fbld:7:12
