
set prg {
  struct Unit();
  union Fruit(Unit apple, Unit banana, Unit pear);
  union Maybe(Unit nothing, Fruit just);

  func main( ; Fruit) {
    Maybe:just(Fruit:pear(Unit())).nosuchfield;
  };
}

expect_malformed $prg main

