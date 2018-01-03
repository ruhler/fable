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

      proc sub( ; ; Bool) {
        $(Bool:true(Unit()));
      };

      proc main( ; ; Bool) {
        # The first port argument has invalid syntax.
        sub( ??? ; );
      };

    };
  }
}

fbld-check-error $prg MainM MainM.fbld:12:14
