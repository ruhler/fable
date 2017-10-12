# Test a simple exec process.
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
        Bool x = $(Bool:true(Unit()));
        $(x);
      };
    };
  }
}

fbld-test $prg "main@MainM" {} {
  return Bool@MainM:true(Unit@MainM())
}
