set prg {
  MainI.fbld {
    mtype MainI {
      struct Unit();
      struct Donut();
      struct A(Unit x, Unit y);

      func main( ; A);
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      struct Unit();
      struct Donut();
      struct A(Unit x, Unit y);

      func main( ; A) {
        # The type of v is Unit, not Donut
        Unit v = Donut();
        A(v, v);
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:9:18
