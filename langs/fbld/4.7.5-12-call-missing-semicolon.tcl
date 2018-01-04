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
        Bool +- put, myget;
        Bool putted = +put(Bool:true(Unit()));
        # The semicolon after the port arguments is missing.
        sub(myget);
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:14:18
