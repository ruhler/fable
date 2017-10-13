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
        # The second argument to A should be of type Donut, not Unit.
        A(Unit(), Unit());
      };
    };
  }
}

fbld-check-error $prg MainM 9:19
