set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      func main( ; Unit);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
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
