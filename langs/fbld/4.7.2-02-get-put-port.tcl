set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      proc f(Unit+ myget ; ; Unit);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();

      proc f(Unit+ myget ; ; Unit) {
        # myget port is a put port, not a get port.
        -myget();
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:7:10
