set prg {
  struct Unit();
  union Fruit(Unit apple, Unit banana, Unit pear);

  proc main( ; ; Fruit) {
    // A condition is missing its final close paren.
    ?(Fruit:pear(Unit()) ;
      $(Fruit:pear(Unit())), $(Fruit:apple(Unit())), $(Fruit:banana(Unit()));
  };
}

expect_malformed $prg main
