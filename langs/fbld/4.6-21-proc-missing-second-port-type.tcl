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

      # The second port argument is missing its type.
      proc p(Unit- px, + py ; Unit x, Unit y ; Unit) {
        $(Unit());
      };

      proc main( ; ; Unit) {
        $(Unit());
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:6:24
