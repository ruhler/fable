set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      union Bool(Unit true, Unit false);
      proc main( ; ; Bool);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();
      union Bool(Unit true, Unit false);

      proc sub( ; Bool x; Bool) {
        ?(x ; $(Bool:false(Unit())), $(Bool:true(Unit())));
      };

      proc main( ; ; Bool) {
        # The argument is malformed.
        sub( ; ???);
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:12:17
