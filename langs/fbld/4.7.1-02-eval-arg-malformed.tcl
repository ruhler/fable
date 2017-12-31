set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      proc main( ; ; Unit);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();

      proc main( ; ; Unit) {
        # The argument to eval has the wrong syntax.
        $(???);
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:7:12
