set prg {
  Main.mtype {
    mtype Main {
      struct Unit();
      func main( ; Unit);
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
      struct Unit();
      struct Donut();
      struct A(Unit x, Donut y);

      func main( ; Unit) {
        # The field name is missing.
        A(Unit(), Donut()).;
      };
    };
  }
}

fbld-check-error $prg Main Main.mdefn:9:28
