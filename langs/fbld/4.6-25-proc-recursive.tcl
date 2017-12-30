set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      union Int(Unit 0, Unit 1, Unit 2);
      proc main( ; ; Int);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();
      union Int(Unit 0, Unit 1, Unit 2);

      proc f( ; Int x ; Int) {
        ?(x ; $(x), f( ; Int:0(Unit())), f( ; Int:1(Unit())));
      };

      # Recursive procs should be fine.
      proc main( ; ; Int) {
        f( ; Int:2(Unit()));
      };
    };
  }
}

fbld-test $prg "main@MainM" {} {
  return Int@MainM:0(Unit@MainM())
}
