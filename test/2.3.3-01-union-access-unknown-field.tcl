set prg {
  struct Unit();
  union Fruit(Unit apple, Unit banana, Unit pear);
  union Maybe(Unit nothing, Fruit just);

  func main( ; Fruit) {
    # nosuchfield is not a field of Maybe
    .nosuchfield(Maybe:just(Fruit:pear(Unit())));
  };
}
fblc-check-error $prg 8:6
