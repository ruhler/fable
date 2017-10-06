set prg {
  Main.mtype {
    mtype Main {
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
      struct Unit();
      union Fruit(Unit apple, Unit banana, Unit pear);
      union Maybe(Unit nothing, Fruit just);

      func main( ; Fruit) {
        # nosuchfield is not a field of Maybe
        Maybe:just(Fruit:pear(Unit())).nosuchfield;
      };
    };
  }
}

fbld-check-error $prg Main Main.mdefn:9:40
