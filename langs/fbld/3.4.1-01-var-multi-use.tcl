set prg {
  Main.mtype {
    mtype Main<> {
      struct Unit();
      union Bool(Unit True, Unit False);
      struct Pair(Bool a, Bool b);
      func main(Bool x ; Pair);
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      struct Unit();
      union Bool(Unit True, Unit False);
      struct Pair(Bool a, Bool b);

      func main(Bool x ; Pair) {
        # Test that a variable can be used in multiple places to share a value.
        Pair(x, x);
      };
    };
  }
}

fbld-test $prg "main@Main<;>" { Bool@Main<;>:True(Unit@Main<;>()) } {
  return Pair@Main<;>(Bool@Main<;>:True(Unit@Main<;>()),Bool@Main<;>:True(Unit@Main<;>()))
}
