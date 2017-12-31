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
        # The close parenthesis is missing.
        +myput(Unit();
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:7:22
