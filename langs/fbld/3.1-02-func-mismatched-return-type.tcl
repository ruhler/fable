set prg {
  MainI.fbld {
    interf MainI {
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      # The return type of the function must match the type of the body.
      struct Unit();
      struct Donut();

      func main( ; Unit) {
        Donut();
      };
    };
  }
}
fbld-check-error $prg MainM MainM.fbld:8:9
