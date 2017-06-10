set prg {
  Main.mdecl {
    mdecl Main() {
      struct Unit();
      struct Donut();
      struct A(Unit x, Donut y);
      union Bool(Unit true, Unit false);
      func main( ; Unit);
    };
  }

  Main.mdefn {
    mdefn Main() {
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
fbld-check-error $prg Main Main.mdefn:10:28
