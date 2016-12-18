
# Test that a get will block for something to be put.
# We can't gaurentee this test will exercise the desired case, because it
# depends on how things are scheduled, but hopefully it will test it.
set prg {
  struct Unit();
  union Fruit(Unit apple, Unit banana, Unit pear);

  proc main( ; ; Fruit) {
    Fruit <~> get, put;
    Fruit p1 = put~(Fruit:pear(Unit())), Fruit g1 = get~();
    Fruit g2 = get~(), Fruit p2 = put~(p1);
    $(g2);
  };
}

expect_result Fruit:pear(Unit()) $prg main
# skip: decide how to id ports: by link or by port?
skip expect_result_b "10" $prg 2

