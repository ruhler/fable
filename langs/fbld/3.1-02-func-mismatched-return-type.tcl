set prg {
  Main.mtype {
    mtype Main<> {
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      # The return type of the function must match the type of the body.
      struct Unit();
      struct Donut();

      func main( ; Unit) {
        Donut();
      };
    };
  }
}
fbld-check-error $prg Main Main.mdefn:8:9
