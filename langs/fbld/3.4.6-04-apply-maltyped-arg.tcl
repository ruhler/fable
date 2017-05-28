set prg {
  Main.mdecl {
    mdecl Main() {
      struct Unit();
      struct Donut();
      func f(Unit x; Unit);
      func main( ; Unit);
    };
  }

  Main.mdefn {
    mdefn Main() {
      struct Unit();
      struct Donut();

      func f(Unit x; Unit) {
        x;
      };

      func main( ; Unit) {
        # The argument should have type Unit, not type Donut
        f(Donut());
      };
    };
  }
}

fbld-check-error $prg Main Main.mdefn:12:11
