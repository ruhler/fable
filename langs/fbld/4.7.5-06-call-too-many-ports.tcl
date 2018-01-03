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

        # sub takes 1 port argument, but two are provided.
        sub(myget, put ; );
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:15:9
