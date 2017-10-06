# Two structs with different names are considered different types, even if
# they have the same fields.
set prg {
  Main.mtype {
    mtype Main {
      struct Unit();
      struct Donut();
      func main( ; Unit);
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
      struct Unit();
      struct Donut();

      func main( ; Unit) {
        Donut();
      };
    };
  }
}

fbld-check-error $prg Main Main.mdefn:7:9

set prg {
  Main.mtype {
    mtype Main {
      struct Unit();
      struct A(Unit x, Unit y);
      struct B(Unit x, Unit y);
      func main( ; A);
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
      struct Unit();

      struct A(Unit x, Unit y);
      struct B(Unit x, Unit y);

      func main( ; A) {
        B(Unit(), Unit());
      };
    };
  }
}

fbld-check-error $prg Main Main.mdefn:9:9
