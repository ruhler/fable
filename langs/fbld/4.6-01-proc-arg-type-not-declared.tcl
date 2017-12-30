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

      # The process argument type Donut is not defined.
      proc f( ; Unit x, Donut y; Unit) {
        $(x);
      };

      proc main( ; ; Unit) {
        $(Unit());
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:6:25
