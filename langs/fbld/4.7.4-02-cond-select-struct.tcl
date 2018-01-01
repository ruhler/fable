set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      struct Pair(Unit a, Unit b, Unit c);
      union Fruit(Unit apple, Unit banana, Unit pear);
      proc main( ; ; Fruit);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();
      struct Pair(Unit a, Unit b, Unit c);
      union Fruit(Unit apple, Unit banana, Unit pear);
      proc main( ; ; Fruit) {
        # The condition is not a union type.
        ?(Pair(Unit(), Unit(), Unit());
          $(Fruit:pear(Unit())), $(Fruit:apple(Unit())), $(Fruit:banana(Unit())));
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:8:11
