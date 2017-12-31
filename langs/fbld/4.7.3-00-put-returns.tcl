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

      proc main( ; ; Bool) {
        # Test that the put process returns the put value.
        Bool +- put, get;
        +put(Bool:true(Unit()));
      };
    };
  }
}

fbld-test $prg "main@MainM" {} {
  return Bool@MainM:true(Unit@MainM())
}
