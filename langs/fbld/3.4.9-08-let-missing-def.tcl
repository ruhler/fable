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
        # The variable definition is missing.
        Unit v = ;
        A(v, v);
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:8:18
