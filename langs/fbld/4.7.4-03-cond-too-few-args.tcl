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
      proc main( ; ; Fruit) {
        # The condition has too few arguments.
        ?(Fruit:pear(Unit()) ;
          $(Fruit:pear(Unit())), $(Fruit:apple(Unit())));
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:7:9
