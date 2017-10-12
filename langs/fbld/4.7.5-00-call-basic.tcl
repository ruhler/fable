# Test a basic process call.
set prg {
  MainI.fbld {
    mtype MainI {
      struct Unit();
      union Bool(Unit true, Unit false);
      proc sub( ; ; Bool);
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      struct Unit();
      union Bool(Unit true, Unit false);
      proc sub( ; ; Bool) {
        $(Bool:true(Unit()));
      };

      proc main( ; ; Bool) {
        sub( ; );
      };
    };
  }
}

fbld-test $prg "main@MainM" {} {
  return Bool@MainM:true(Unit@MainM())
}
