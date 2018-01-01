set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      union Bool(Unit true, Unit false);
      union Fruit(Unit apple, Unit banana, Unit pear);
      proc main( ; ; Fruit);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();
      union Bool(Unit true, Unit false);
      union Fruit(Unit apple, Unit banana, Unit pear);
      proc main( ; ; Fruit) {
        # The variable x is not declared
        ?(Bool:true(Unit()) ; $(Fruit:pear(Unit())), $(x));
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:8:56
