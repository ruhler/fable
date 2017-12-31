set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      proc f(Unit- myput ; ; Unit);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();

      proc f(Unit- myput ; ; Unit) {
        # myput port has the wrong polarity
        +myput(Unit());
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:7:10
