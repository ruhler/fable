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

fbld-test $prg Main@main { Main@Bool:True(Main@Unit()) } "return Main@Bool:True(Main@Unit())"
fbld-test $prg Main@main { Main@Bool:False(Main@Unit()) } "return Main@Bool:False(Main@Unit())"
