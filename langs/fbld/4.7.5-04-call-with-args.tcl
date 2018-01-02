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
        sub( ; Bool:false(Unit()));
      };
    };
  }
}

fbld-test $prg main@MainM {} {
  return Bool@MainM:true(Unit@MainM())
}
