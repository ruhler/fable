set prg {
  interf MainI {
    struct Unit();
    struct Donut();
    func main( ; Unit);
  };

  module MainM(MainI) {
    # The Donut type is not declared locally.
    struct Unit();
    func main( ; Unit) {
      Unit();
    };
  };
}

fbld-check-error $prg 8:10
