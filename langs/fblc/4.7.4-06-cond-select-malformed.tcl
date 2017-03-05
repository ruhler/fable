set prg {
  struct Unit();
  union Fruit(Unit apple, Unit banana, Unit pear);

  proc main( ; ; Fruit) {
    # The select is malformed.
    ?( ??? ;
      $(Fruit:pear(Unit())), $(Fruit:apple(Unit())), $(Fruit:banana(Unit())));
  };
}
fblc-check-error $prg 7:9
