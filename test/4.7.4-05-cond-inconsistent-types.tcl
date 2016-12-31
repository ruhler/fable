set prg {
  struct Unit();
  union Fruit(Unit apple, Unit banana, Unit pear);

  proc main( ; ; Fruit) {
    // The condition argument types are not consistent.
    ?(Fruit:pear(Unit()) ;
      $(Fruit:pear(Unit())), $(Fruit:apple(Unit())), $(Unit()));
  };
}
# TODO: Error location should be the $ sign, not the eval expression?
fblc-check-error $prg 8:56
