set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      proc main(Unit+ out ; ; Unit);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();

      proc main(Unit+ out ; ; Unit) {
        # The port name is missing
        + (Unit());
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:7:11
