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

      proc sub(Bool+ put ; ; Unit) {
        Bool putted = +put(Bool:true(Unit()));
        $(Unit());
      };

      # Test process call that takes a put port.
      proc main( ; ; Bool) {
        Bool +- myput, myget;
        Unit blah = sub(myput ; );
        -myget();
      };
    };
  }
}

fbld-test $prg main@MainM {} {
  return Bool@MainM:true(Unit@MainM())
}
