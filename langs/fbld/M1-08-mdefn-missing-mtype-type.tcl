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
      # The Donut type is not declared locally.
      struct Unit();
      func main( ; Unit) {
        Unit();
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:2:11
