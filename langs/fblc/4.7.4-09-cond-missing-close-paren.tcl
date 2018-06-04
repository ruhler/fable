set prg {
  struct Unit();
  union Fruit(Unit apple, Unit banana, Unit pear);

  proc main( ; ; Fruit) {
    # A condition is missing its final close paren.
    ?(Fruit:pear(Unit()) ;
      apple: $(Fruit:pear(Unit())),
      banana: $(Fruit:apple(Unit())),
      pear: $(Fruit:banana(Unit()));
  };
}
fblc-check-error $prg 10:36
