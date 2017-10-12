# Two structs with different names are considered different types, even if
# they have the same fields.
set prg {
  MainI.fbld {
    mtype MainI {
      struct Unit();
      struct Donut();
      func main( ; Unit);
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      struct Unit();
      struct Donut();

      func main( ; Unit) {
        Donut();
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:7:9

set prg {
  MainI.fbld {
    mtype MainI {
      struct Unit();
      struct A(Unit x, Unit y);
      struct B(Unit x, Unit y);
      func main( ; A);
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      struct Unit();

      struct A(Unit x, Unit y);
      struct B(Unit x, Unit y);

      func main( ; A) {
        B(Unit(), Unit());
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:9:9
