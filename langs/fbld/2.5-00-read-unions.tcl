set prg {
  Main.mtype {
    mtype Main {
      struct Unit();
      union Bool(Unit True, Unit False);
      proc main(Bool- in ; ; Bool);
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
      struct Unit();
      union Bool(Unit True, Unit False);

      proc main(Bool- in ; ; Bool) {
        Bool x = -in();
        -in();
      };
    };
  }
}

# Verify we can read two union values in a row.
fbld-test $prg "main@Main" {} {
  put in Bool@Main:False(Unit@Main())
  put in Bool@Main:True(Unit@Main())
  return Bool@Main:True(Unit@Main())
}
