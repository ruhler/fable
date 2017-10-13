set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      struct Donut();
      struct A(Unit x, Donut y);
      union Bool(Unit true, Unit false);
      func main( ; Unit);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();
      struct Donut();
      struct A(Unit x, Donut y);
      union Bool(Unit true, Unit false);

      func main( ; Unit) {
        # You can't access a struct with a conditional expression.
        A(Unit(), Donut()).?(Bool:true(Unit() ; Unit(), Unit()));
      };
    };
  }
}
fbld-check-error $prg MainM MainM.fbld:10:28
