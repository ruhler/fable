# Test calling a process with multiple ports.
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

fbld-test $prg "main@Main" {} {
  return Bool@Main:true(Unit@Main())
}
