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
        # The variable 'x' is not declared
        +myput(x);
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:7:16
