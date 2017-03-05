set prg {
  struct Unit();
  union Fruit(Unit apple, Unit banana, Unit pear);

  proc main( ; ; Fruit) {
    # The condition argument types are not consistent.
    ?(Fruit:pear(Unit()) ;
      $(Fruit:pear(Unit())), $(Fruit:apple(Unit())), $(Unit()));
  };
}
fblc-check-error $prg 8:54
