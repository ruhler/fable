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

      # A port type has not been declared.
      proc p(Donut- px, Unit+ py ; Unit x, Unit y ; Unit) {
        $(Unit());
      };

      proc main( ; ; Unit) {
        $(Unit());
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:6:14
