set prg {
  struct Unit();
  union Fruit(Unit apple, Unit banana, Unit pear);

  proc main( ; ; Fruit) {
    // A condition arg is malformed.
    ?(Fruit:pear(Unit()) ;
      $(Fruit:pear(Unit())), ???, $(Fruit:banana(Unit())));
  };
}

expect_malformed $prg main
