# Access a component of a union.
set prg {
  Main.mtype {
    mtype Main {
      struct Unit();
      union Fruit(Unit apple, Unit banana, Unit pear);
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
      struct Unit();
      union Fruit(Unit apple, Unit banana, Unit pear);
      union Maybe(Unit nothing, Fruit just);

      func main( ; Fruit) {
        Maybe:just(Fruit:pear(Unit())).just;
      };
    };
  }
}

fbld-test $prg "main@Main" {} {
  return Fruit@Main:pear(Unit@Main())
}
