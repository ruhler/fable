set prg {
  struct Unit();
  union Fruit(Unit apple, Unit banana, Unit pear);

  proc main( ; ; Fruit) {
    # The conditional arguments are in the wrong order.
    ?(Fruit:pear(Unit()) ;
      banana: $(Fruit:apple(Unit())),
      apple: $(Fruit:pear(Unit())),
      pear: $(Fruit:banana(Unit())));
  };
}

fblc-check-error $prg 8:7
