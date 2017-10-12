set prg {
  MainI.fbld {
    mtype MainI {
      struct Unit();
      union Bool(Unit True, Unit False);
      func main(Bool x ; Bool);
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      struct Unit();
      union Bool(Unit True, Unit False);

      func main(Bool x ; Bool) {
        x;
      };
    };
  }
}

fbld-test $prg "main@MainM" { Bool@MainM:True(Unit@MainM()) } {
  return Bool@MainM:True(Unit@MainM())
}

fbld-test $prg "main@MainM" { Bool@MainM:False(Unit@MainM()) } {
  return Bool@MainM:False(Unit@MainM())
}
