set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      proc main( ; ; Unit);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();
      struct Donut();

      # The return type of a process doesn't match the type of the body.
      proc main( ; ; Unit) {
        $(Donut());
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:8:9
