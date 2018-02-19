set prg {
  struct Unit();

  module Food {
    # Importing from parent makes Unit visible in module Food.
    import @ { Unit; };

    union Fruit(Unit apple, Unit banana);
  };

  func main( ; Unit) {
    Unit();
  };
}

fbld-test $prg "main" {} {
  return Unit()
}
