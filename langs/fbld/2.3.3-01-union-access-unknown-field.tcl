set prg {
  MainI.fbld {
    mtype MainI {
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
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

fbld-check-error $prg MainM MainM.fbld:9:40
