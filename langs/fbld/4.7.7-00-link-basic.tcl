# Test a simple link process.
set prg {
  Main.mtype {
    mtype Main {
      struct Unit();
      union Bool(Unit true, Unit false);
      proc main( ; ; Bool);
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
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

fbld-test $prg "main@Main" {} {
  return Bool@Main:true(Unit@Main())
}
