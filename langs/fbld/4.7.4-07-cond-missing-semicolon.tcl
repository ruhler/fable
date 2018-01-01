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
        # The semicolon is missing separating the select from the arguments.
        ?(Fruit:pear(Unit()) 
          $(Fruit:pear(Unit())), $(Fruit:apple(Unit())), $(Fruit:banana(Unit())));
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:8:11
