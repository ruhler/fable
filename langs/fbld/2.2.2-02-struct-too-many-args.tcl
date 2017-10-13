set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      struct Donut();
      struct A(Unit x, Donut y);
      func main( ; A);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();
      struct Donut();
      struct A(Unit x, Donut y);

      func main( ; A) {
        # There should be two arguments provided, not three.
        A(Unit(), Donut(), Unit());
      };
    };
  }
}

fbld-check-error $prg MainM 9:9
