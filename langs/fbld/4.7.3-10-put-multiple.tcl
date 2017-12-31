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

      # Test that you can put twice to the same port before getting.
      proc main( ; ; Bool) {
        Bool +- put, get;
        Bool p1 = +put(Bool:false(Unit()));
        Bool p2 = +put(Bool:true(Unit()));
        Bool g1 = -get();
        -get();
      };
    };
  }
}

fbld-test $prg main@MainM {} {
  return Bool@MainM:true(Unit@MainM())
}
