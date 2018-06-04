set prg {
  struct Unit();
  union Int(Unit 0, Unit 1, Unit 2);

  proc f( ; Int x ; Int) {
    ?(x ; 0: $(x), 1: f( ; Int:0(Unit())), 2: f( ; Int:1(Unit())));
  };

  # Recursive procedures should be fine.
  proc main( ; ; Int) {
    f( ; Int:2(Unit()));
  };
}

fblc-test $prg main {} "return Int:0(Unit())"
