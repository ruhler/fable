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
        # Calling with a Unit when a Bool is expected.
        sub( ; Unit());
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:12:16
