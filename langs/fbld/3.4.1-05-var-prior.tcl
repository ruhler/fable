set prg {
  Main.mdecl {
    mdecl Main() {
      struct Unit();
      union Bool(Unit True, Unit False);
      func main(Bool x, Bool y; Bool);
    };
  }

  Main.mdefn {
    mdefn Main() {
      struct Unit();
      union Bool(Unit True, Unit False);

      func main(Bool x, Bool y; Bool) {
        x;  # x is not the most recent variable on the stack.
      };
    };
  }
}

fbld-test $prg main@Main { Bool@Main:False(Unit@Main()) Bool@Main:True(Unit@Main()) } {
    return Bool@Main:False(Unit@Main())
}
fbld-test $prg main@Main { Bool@Main:True(Unit@Main()) Bool@Main:False(Unit@Main()) } {
    return Bool@Main:True(Unit@Main())
}
