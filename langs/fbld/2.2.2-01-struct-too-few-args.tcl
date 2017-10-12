set prg {
  MainI.fbld {
    mtype MainI {
      struct Unit();
      struct Donut();
      struct A(Unit x, Donut y);
      func main( ; A);
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      struct Unit();
      struct Donut();
      struct A(Unit x, Donut y);

      func main( ; A) {
        # There should be two arguments provided, not one.
        A(Unit());
      };
    };
  }
}

fbld-check-error $prg MainM 9:9
