set prg {
  struct Unit();
  union Fruit(Unit apple, Unit banana, Unit pear);

  proc main( ; ; Fruit) {
    # A condition arg is malformed.
    ?(Fruit:pear(Unit()) ;
      apple: $(Fruit:pear(Unit())),
      banana: ???,
      pear: $(Fruit:banana(Unit())));
  };
}
fblc-check-error $prg 9:16
