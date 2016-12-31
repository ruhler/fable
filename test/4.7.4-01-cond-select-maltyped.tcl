set prg {
  struct Unit();
  union Bool(Unit true, Unit false);
  union Fruit(Unit apple, Unit banana, Unit pear);

  proc main( ; ; Fruit) {
    // The select expression is maltyped.
    ?( x ;
      $(Fruit:pear(Unit())), $(Fruit:apple(Unit())), $(Fruit:banana(Unit())));
  };
}
fblc-check-error $prg 8:8
