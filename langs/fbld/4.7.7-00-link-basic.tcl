# Test a simple link process.
set prg {
  MainI.fbld {
    mtype MainI {
      struct Unit();
      union Bool(Unit true, Unit false);
      proc main( ; ; Bool);
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      struct Unit();
      union Bool(Unit true, Unit false);

      proc main( ; ; Bool) {
        Bool +- put, get;
        Bool putted = +put(Bool:true(Unit()));
        -get();
      };
    };
  }
}

fbld-test $prg "main@MainM" {} {
  return Bool@MainM:true(Unit@MainM())
}
