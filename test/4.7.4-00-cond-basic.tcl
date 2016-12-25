
# Test a basic conditional process.
set prg {
  struct Unit();
  union Fruit(Unit apple, Unit banana, Unit pear);

  proc main( ; ; Fruit) {
    ?(Fruit:pear(Unit()) ;
      $(Fruit:pear(Unit())), $(Fruit:apple(Unit())), $(Fruit:banana(Unit())));
  };
}

expect_result Fruit:banana(Unit()) $prg main
expect_result_b 01 $prg 2

