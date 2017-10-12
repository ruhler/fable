set prg {
  MainI.fbld {
    mtype MainI {
      struct Unit();
      union Bool(Unit True, Unit False);
      proc main(Bool- in ; ; Bool);
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
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
fbld-test $prg "main@MainM" {} {
  put in Bool@MainM:False(Unit@MainM())
  put in Bool@MainM:True(Unit@MainM())
  return Bool@MainM:True(Unit@MainM())
}
