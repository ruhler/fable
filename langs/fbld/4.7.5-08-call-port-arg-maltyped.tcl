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

      proc sub(Unit- get ; ; Unit) {
        -get();
      };

      proc main( ; ; Bool) {
        Bool +- put, myget;
        Bool putted = +put(Bool:true(Unit()));

        # sub takes a port of type Unit, not Bool.
        sub(myget ; );
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:15:13
