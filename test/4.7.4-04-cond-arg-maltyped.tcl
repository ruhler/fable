
set prg {
  struct Unit();
  union Bool(Unit true, Unit false);
  union Fruit(Unit apple, Unit banana, Unit pear);

  proc main( ; ; Fruit) {
    // The variable x is not declared.
    ?(Bool:true(Unit()) ; $(Fruit:pear(Unit())), $(x));
  };
}

expect_malformed $prg main

