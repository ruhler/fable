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
        # myget port is not in scope.
        -myget();
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:7:10
