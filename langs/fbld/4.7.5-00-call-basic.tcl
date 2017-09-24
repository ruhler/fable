# Test a basic process call.
set prg {
  Main.mtype {
    mtype Main<> {
      struct Unit();
      union Bool(Unit true, Unit false);
      proc sub( ; ; Bool);
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
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

fbld-test $prg "main@Main<;>" {} {
  return Bool@Main<;>:true(Unit@Main<;>())
}
