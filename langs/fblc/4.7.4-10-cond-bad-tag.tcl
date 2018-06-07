set prg {
  struct Unit();
  union Fruit(Unit apple, Unit banana, Unit pear);

  proc main( ; ; Fruit) {
    # 'bogus' is not the right tag.
    ?(Fruit:pear(Unit()) ;
      apple: $(Fruit:pear(Unit())),
      bogus: $(Fruit:apple(Unit())),
      pear: $(Fruit:banana(Unit())));
  };
}

fblc-check-error $prg 9:7
