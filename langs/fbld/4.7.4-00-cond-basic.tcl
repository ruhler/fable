# Test a basic conditional process.
set prg {
  Main.mtype {
    mtype Main<> {
      struct Unit();
      union Fruit(Unit apple, Unit banana, Unit pear);
      proc main( ; ; Fruit);
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      struct Unit();
      union Fruit(Unit apple, Unit banana, Unit pear);
      proc main( ; ; Fruit) {
        ?(Fruit:pear(Unit()) ;
          $(Fruit:pear(Unit())), $(Fruit:apple(Unit())), $(Fruit:banana(Unit())));
      };
    };
  }
}

fbld-test $prg "main@Main<;>" {} {
  return Fruit@Main<;>:banana(Unit@Main<;>())
}
