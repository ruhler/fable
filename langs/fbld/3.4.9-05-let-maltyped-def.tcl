set prg {
  Main.mtype {
    mtype Main {
      struct Unit();
      struct Donut();
      struct A(Unit x, Unit y);

      func main( ; A);
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
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

fbld-check-error $prg Main Main.mdefn:9:18
