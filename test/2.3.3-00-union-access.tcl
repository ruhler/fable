set prg {
  # Access a component of a union.
  struct Unit();
  union Fruit(Unit apple, Unit banana, Unit pear);
  union Maybe(Unit nothing, Fruit just);

  func main( ; Fruit) {
    .just(Maybe:just(Fruit:pear(Unit())));
  };
}
expect_result Fruit:pear(Unit()) $prg main
expect_result_b 10 $prg 3
