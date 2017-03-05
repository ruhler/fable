set prg {
  struct Unit();
  union Fruit(Unit apple, Unit banana, Unit pear);

  proc main( ; ; Fruit) {
    # The semicolon is missing separating the select from the arguments.
    ?(Fruit:pear(Unit()) 
      $(Fruit:pear(Unit())), $(Fruit:apple(Unit())), $(Fruit:banana(Unit())));
  };
}
fblc-check-error $prg 8:7
