# Test a basic conditional process.
set prg {
  MainI.fbld {
    mtype MainI {
      struct Unit();
      union Fruit(Unit apple, Unit banana, Unit pear);
      proc main( ; ; Fruit);
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      struct Unit();
      union Fruit(Unit apple, Unit banana, Unit pear);
      proc main( ; ; Fruit) {
        ?(Fruit:pear(Unit()) ;
          $(Fruit:pear(Unit())), $(Fruit:apple(Unit())), $(Fruit:banana(Unit())));
      };
    };
  }
}

fbld-test $prg "main@MainM" {} {
  return Fruit@MainM:banana(Unit@MainM())
}
