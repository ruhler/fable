set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      struct Donut();
      proc f(Donut- myget ; ; Unit);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();
      struct Donut();

      proc f(Donut- myget ; ; Unit) {
        # myget port has the wrong type.
        -myget();
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:8:9
