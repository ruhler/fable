set prg {
  struct Unit();
  struct Donut();

  module Food {
    # Multiple entities can be imported.
    import @ { Unit; Donut; };

    union Fruit(Unit apple, Donut banana);
  };

  func main( ; Unit) {
    Unit();
  };
}

fbld-test $prg "main" {} {
  return Unit()
}
