set prg {
  Main.mdecl {
    mdecl Main() {
      struct Unit();
      union Bool(Unit True, Unit False);
      func main(Bool x ; Bool);
    };
  }

  Main.mdefn {
    mdefn Main() {
      struct Unit();
      union Bool(Unit True, Unit False);

      func main(Bool x ; Bool) {
        x;
      };
    };
  }
}

skip fbld-test $prg main@Main { Bool@Main:True(Unit@Main()) } "return Bool@Main:True(Unit@Main())"
skip fbld-test $prg main@Main { Bool@Main:False(Unit@Main()) } "return Bool@Main:False(Unit@Main())"
