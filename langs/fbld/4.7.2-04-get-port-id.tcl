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

      # Test that references to get ports are properly linked with their source.
      proc sub(Bool+ put, Bool- get; ; Bool) {
        -get();
      };

      proc main( ; ; Bool) {
        Bool +- p1, g1;
        Bool +- p2, g2;
        Bool x1 = +p1(Bool:true(Unit()));
        Bool x2 = +p2(Bool:false(Unit()));
        sub(p1, g2 ; ); 
      };
    };
  }
}

fbld-test $prg "main@MainM" {} {
  return Bool@MainM:false(Unit@MainM())
}
