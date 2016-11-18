
set prg {
  struct Unit();
  struct Pair(Unit a, Unit b, Unit c);
  union Fruit(Unit apple, Unit banana, Unit pear);

  proc main( ; ; Fruit) {
    // The condition is not a union type.
    ?(Pair(Unit(), Unit(), Unit()) ;
      $(Fruit:pear(Unit())), $(Fruit:apple(Unit())), $(Fruit:banana(Unit())));
  };
}

expect_malformed $prg main

