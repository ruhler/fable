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
        # The variable x is not in scope.
        $(x);
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:7:11
