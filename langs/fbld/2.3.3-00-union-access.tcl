# Access a component of a union.
set prg {
  MainI.fbld {
    mtype MainI {
      struct Unit();
      union Fruit(Unit apple, Unit banana, Unit pear);
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
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
