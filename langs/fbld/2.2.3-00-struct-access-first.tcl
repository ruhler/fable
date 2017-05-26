set prg {
  Main.mdecl {
    mdecl Main() {
      struct Unit();
      func main( ; Unit);
    };
  }

  Main.mdefn {
    mdefn Main() {
      # Accessing the first component of a struct.
      struct Unit();
      struct Donut();
      struct A(Unit x, Donut y);

      func main( ; Unit) {
        A(Unit(), Donut()).x;
      };
    };
  }
}

fbld-test $prg main@Main {} "return Unit@Main()"
