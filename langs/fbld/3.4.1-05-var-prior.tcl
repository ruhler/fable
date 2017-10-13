set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      union Bool(Unit True, Unit False);
      func main(Bool x, Bool y; Bool);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();
      union Bool(Unit True, Unit False);

      func main(Bool x, Bool y; Bool) {
        x;  # x is not the most recent variable on the stack.
      };
    };
  }
}

fbld-test $prg "main@MainM" { Bool@MainM:False(Unit@MainM()) Bool@MainM:True(Unit@MainM()) } {
    return Bool@MainM:False(Unit@MainM())
}

fbld-test $prg "main@MainM" { Bool@MainM:True(Unit@MainM()) Bool@MainM:False(Unit@MainM()) } {
    return Bool@MainM:True(Unit@MainM())
}
