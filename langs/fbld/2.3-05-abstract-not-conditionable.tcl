set prg {
  module StdLib {
    struct Unit();
    abst union Fruit(Unit apple, Unit banana);

    func MkApple( ; Fruit) {
      Fruit:apple(Unit());
    };
  };

  import StdLib { Unit; Fruit; MkApple; };

  func main( ; Unit) {
    # Fruit cannot be used as the argument of a condition because it is
    # abstract.
    ?(MkApple(); Unit(), Unit());
  };
}

skip fbld-check-error $prg 16:7
