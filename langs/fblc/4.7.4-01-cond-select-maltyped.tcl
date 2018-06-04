set prg {
  struct Unit();
  union Fruit(Unit apple, Unit banana, Unit pear);

  proc main( ; ; Fruit) {
    # The select expression is maltyped.
    ?( x ;
      apple: $(Fruit:pear(Unit())),
      banana: $(Fruit:apple(Unit())),
      pear: $(Fruit:banana(Unit())));
  ;
}
fblc-check-error $prg 7:8
