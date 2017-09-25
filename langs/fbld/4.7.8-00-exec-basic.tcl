# Test a simple exec process.
set prg {
  Main.mtype {
    mtype Main<> {
      struct Unit();
      union Bool(Unit true, Unit false);
      proc main( ; ; Bool);
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      struct Unit();
      union Bool(Unit true, Unit false);
      proc main( ; ; Bool) {
        Bool x = $(Bool:true(Unit()));
        $(x);
      };
    };
  }
}

fbld-test $prg "main@Main<;>" {} {
  return Bool@Main<;>:true(Unit@Main<;>())
}
