set prg {
  Main.mtype {
    mtype Main<> {
      struct Unit();
      struct Donut();
      struct A(Unit x, Donut y);
      func main( ; A);
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
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

fbld-check-error $prg Main 9:9
