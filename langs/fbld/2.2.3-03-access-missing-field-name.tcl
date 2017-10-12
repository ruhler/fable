set prg {
  MainI.fbld {
    mtype MainI {
      struct Unit();
      func main( ; Unit);
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
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

fbld-check-error $prg MainM MainM.fbld:9:28
