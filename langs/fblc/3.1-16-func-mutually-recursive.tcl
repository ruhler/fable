set prg {
  struct Unit();
  union Int(Unit 0, Unit 1, Unit 2);

  func f(Int x ; Int) {
    ?(x ; 0: x, 1; h(Int:0(Unit())), 2: h(Int:1(Unit())));
  };

  func h(Int x ; Int) {
    ?(x ; 0: x, 1: f(Int:0(Unit())), 2: f(Int:1(Unit())));
  };

  # Mutually recursive functions should be fine.
  func main( ; Int) {
    f(Int:2(Unit()));
  };
}

fblc-test $prg main {} "return Int:0(Unit())"
