set prg {
  # Test a basic conditional process.
  struct Unit();
  union Fruit(Unit apple, Unit banana, Unit pear);

  proc main( ; ; Fruit) {
    ?(Fruit:pear(Unit()) ;
      $(Fruit:pear(Unit())), $(Fruit:apple(Unit())), $(Fruit:banana(Unit())));
  };
}

fblc-test $prg main {} "return Fruit:banana(Unit())"
