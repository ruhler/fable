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
        # The let argument has a syntax error.
        Unit v = ???;
        A(v, v);
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:8:19
