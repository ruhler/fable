
# Access a component of a union.
set prg {
  struct Unit();
  union Fruit(Unit apple, Unit banana, Unit pear);
  union Maybe(Unit nothing, Fruit just);

  func main( ; Fruit) {
    Maybe:just(Fruit:pear(Unit())).just;
  };
}
expect_result Fruit:pear(Unit()) $prg main
skip expect_result_b "10" $prg 3

