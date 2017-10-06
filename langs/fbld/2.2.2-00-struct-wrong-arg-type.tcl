set prg {
  Main.mtype {
    mtype Main {
      struct Unit();
      struct Donut();
      struct A(Unit x, Donut y);
      func main( ; A);
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
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

fbld-check-error $prg Main 9:19
