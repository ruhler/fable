set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      struct Donut();
      func f(Unit x; Unit);
      func main( ; Unit);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();
      struct Donut();

      func f(Unit x; Unit) {
        x;
      };

      func main( ; Unit) {
        # The argument should have type Unit, not type Donut
        f(Donut());
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:12:11
