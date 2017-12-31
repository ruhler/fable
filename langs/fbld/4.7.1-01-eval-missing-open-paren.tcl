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
        # The open paren is missing
        $ Unit());
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:7:11
