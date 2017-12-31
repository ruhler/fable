set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      proc f(Unit+ myput ; ; Unit);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();

      proc f(Unit+ myput ; ; Unit) {
        # The argument has invalid syntax
        +myput(???);
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:7:17
