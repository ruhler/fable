set prg {
  interf I {
    struct Unit();
  };

  module M(I) {
    struct Unit();

    # Donut does not appear in the interface, it should not be public here.
    struct Donut();
  };
}

fbld-check-error $prg 10:12
