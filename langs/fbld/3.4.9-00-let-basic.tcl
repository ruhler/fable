set prg {
  MainI.fbld {
    mtype MainI {
      struct Unit();
      struct A(Unit x, Unit y);
      func main( ; A);
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      struct Unit();
      struct A(Unit x, Unit y);

      func main( ; A) {
        Unit v = Unit();
        A(v, v);
      };
    };
  }
}

fbld-test $prg "main@MainM" {} {
  return A@MainM(Unit@MainM(),Unit@MainM())
}
