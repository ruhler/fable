set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      proc f( ; ; Unit);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();

      proc f( ; ; Unit) {
        # myput port is not in scope.
        +myput(Unit());
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:7:10
