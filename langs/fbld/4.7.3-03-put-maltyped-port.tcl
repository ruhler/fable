set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      struct Donut();
      proc f(Donut+ myput ; ; Unit);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();
      struct Donut();

      proc f(Donut+ myput ; ; Unit) {
        # myput has the wrong type
        +myput(Donut());
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:8:9
