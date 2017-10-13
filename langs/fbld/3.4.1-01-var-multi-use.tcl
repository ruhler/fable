set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      union Bool(Unit True, Unit False);
      struct Pair(Bool a, Bool b);
      func main(Bool x ; Pair);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
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

fbld-test $prg "main@MainM" { Bool@MainM:True(Unit@MainM()) } {
  return Pair@MainM(Bool@MainM:True(Unit@MainM()),Bool@MainM:True(Unit@MainM()))
}
