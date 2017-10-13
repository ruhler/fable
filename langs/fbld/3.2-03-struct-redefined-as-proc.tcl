set prg {
  MainI.fbld {
    interf MainI {
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      # A struct and a process can't have the same name.
      struct Unit();
      struct A(Unit x, Unit y);

      proc A( ; Unit a, Unit b ; Unit) {
        $(Unit());
      };
    };
  }
}
fbld-check-error $prg MainM MainM.fbld:7:12
