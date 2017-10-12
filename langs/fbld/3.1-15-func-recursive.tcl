set prg {
  MainI.fbld {
    mtype MainI {
      struct Unit();
      union Int(Unit 0, Unit 1, Unit 2);
      func main( ; Int);
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      struct Unit();
      union Int(Unit 0, Unit 1, Unit 2);

      func f(Int x ; Int) {
        ?(x ; x, f(Int:0(Unit())), f(Int:1(Unit())));
      };

      # Recursive functions should be fine.
      func main( ; Int) {
        f(Int:2(Unit()));
      };
    };
  }
}

fbld-test $prg "main@MainM" {} {
  return Int@MainM:0(Unit@MainM())
}
