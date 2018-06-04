set prg {
  # Test a basic conditional process.
  struct Unit();
  union Fruit(Unit apple, Unit banana, Unit pear);

  proc main( ; ; Fruit) {
    ?(Fruit:pear(Unit()) ;
      apple: $(Fruit:pear(Unit())),
      banana: $(Fruit:apple(Unit())),
      pear: $(Fruit:banana(Unit())));
  };
}

fblc-test $prg main {} "return Fruit:banana(Unit())"
