set prg {
  struct Unit();

  module Food {
    # Unit is not accessible to module Food without explicit import.
    union Fruit(Unit apple, Unit banana);
  };
}

fbld-check-error $prg 6:17
