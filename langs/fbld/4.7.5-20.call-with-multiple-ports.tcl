# Test calling a process with multiple ports.
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

      proc sub(Bool- get, Bool+ put ; ; Bool) {
        Bool b = -get();
        +put(b);
      };

      proc main( ; ; Bool) {
        Bool +- myput, myget;
        Bool putted = +myput(Bool:true(Unit()));
        sub(myget, myput; );
      };
    };
  }
}

fbld-test $prg "main@MainM" {} {
  return Bool@MainM:true(Unit@MainM())
}
