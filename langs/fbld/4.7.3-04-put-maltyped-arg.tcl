set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      struct Donut();
      proc f(Unit+ myput ; ; Unit);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();
      struct Donut();

      proc f(Unit+ myput ; ; Unit) {
        # The argument to myput has the wrong type.
        +myput(Donut());
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:8:16
