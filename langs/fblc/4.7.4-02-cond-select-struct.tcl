set prg {
  struct Unit();
  struct Pair(Unit a, Unit b, Unit c);
  union Fruit(Unit apple, Unit banana, Unit pear);

  proc main( ; ; Fruit) {
    # The condition is not a union type.
    ?(Pair(Unit(), Unit(), Unit()) ;
      apple: $(Fruit:pear(Unit())),
      banana: $(Fruit:apple(Unit())),
      pear: $(Fruit:banana(Unit())));
  };
}
fblc-check-error $prg 8:7
