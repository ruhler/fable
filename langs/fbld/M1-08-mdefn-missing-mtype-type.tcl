set prg {
  Main.mtype {
    mtype Main<> {
      struct Unit();
      struct Donut();
      func main( ; Unit);
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      # The Donut type is not declared locally.
      struct Unit();
      func main( ; Unit) {
        Unit();
      };
    };
  }
}

# TODO: Where should the error be reported?
skip fbld-check-error $prg Main Main:2:11
