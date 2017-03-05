set prg {
  # Access a component of a union.
  struct Unit();
  union Fruit(Unit apple, Unit banana, Unit pear);
  union Maybe(Unit nothing, Fruit just);

  func main( ; Fruit) {
    Maybe:just(Fruit:pear(Unit())).just;
  };
}

fblc-test $prg main {} "return Fruit:pear(Unit())"
