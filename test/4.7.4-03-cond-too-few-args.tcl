set prg {
  struct Unit();
  union Fruit(Unit apple, Unit banana, Unit pear);

  proc main( ; ; Fruit) {
    # The condition has too few arguments.
    ?(Fruit:pear(Unit()) ;
      $(Fruit:pear(Unit())), $(Fruit:apple(Unit())));
  };
}
# TODO: Where exactly should the error location be?
fblc-check-error $prg 7:5
