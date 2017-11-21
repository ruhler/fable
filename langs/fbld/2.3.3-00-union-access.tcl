# Access a component of a union.
set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      union Fruit(Unit apple, Unit banana, Unit pear);
      func main( ; Fruit);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();
      union Fruit(Unit apple, Unit banana, Unit pear);
      union Maybe(Unit nothing, Fruit just);

      func main( ; Fruit) {
        Maybe:just(Fruit:pear(Unit())).just;
      };
    };
  }
}

fbld-test $prg "main@MainM" {} {
  return Fruit@MainM:pear(Unit@MainM())
}
