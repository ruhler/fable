set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      struct A(Unit x, Unit y);
      func main( ; A);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
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
