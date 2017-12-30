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

      # The process declaration is missing the final semicolon.
      proc p(Unit- px, Unit+ py ; Unit x, Unit y ; Unit) {
        $(Unit());
      }

      proc main( ; ; Unit) {
        $(Unit());
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:10:7
