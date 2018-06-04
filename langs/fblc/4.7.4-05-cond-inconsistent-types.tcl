set prg {
  struct Unit();
  union Fruit(Unit apple, Unit banana, Unit pear);

  proc main( ; ; Fruit) {
    # The condition argument types are not consistent.
    ?(Fruit:pear(Unit()) ;
      apple: $(Fruit:pear(Unit())),
      banana: $(Fruit:apple(Unit())),
      pear: $(Unit()));
  };
}
fblc-check-error $prg 10:13
