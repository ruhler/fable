set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      union Fruit(Unit apple, Unit banana, Unit pear);
      proc main( ; ; Fruit);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();
      union Fruit(Unit apple, Unit banana, Unit pear);

      # Test that a get will block for something to be put.
      # We can't guarantee this test will exercise the desired case, because it
      # depends on how things are scheduled, but hopefully it will test it.
      proc main( ; ; Fruit) {
        Fruit +- put, get;
        Fruit p1 = +put(Fruit:pear(Unit())), Fruit g1 = -get();
        Fruit g2 = -get(), Fruit p2 = +put(p1);
        $(g2);
      };
    };
  }
}

fbld-test $prg "main@MainM" {} {
  return Fruit@MainM:pear(Unit@MainM())
}
