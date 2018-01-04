set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      union Bool(Unit true, Unit false);
      proc main( ; ; Bool);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();
      union Bool(Unit true, Unit false);

      proc sub(Bool- get ; ; Bool) {
        -get();
      };

      proc main( ; ; Bool) {
        # The first port argument is not in scope.
        sub(foo ; );
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:12:13
