set prg {
  module Child {
    struct Unit();
  };

  # Unit is not accessible directly from child module.
  union Fruit(Unit apple, Unit banana);
}

fbld-check-error $prg 7:15
