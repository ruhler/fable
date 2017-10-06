set prg {
  Main.mtype {
    mtype Main {
      struct Unit();
      union Bool(Unit True, Unit False);
      func main(Bool x ; Bool);
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
      struct Unit();
      union Bool(Unit True, Unit False);

      func main(Bool x ; Bool) {
        x;
      };
    };
  }
}

fbld-test $prg "main@Main" { Bool@Main:True(Unit@Main()) } {
  return Bool@Main:True(Unit@Main())
}

fbld-test $prg "main@Main" { Bool@Main:False(Unit@Main()) } {
  return Bool@Main:False(Unit@Main())
}
