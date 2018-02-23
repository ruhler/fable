set prg {
  module Food {
    # The type 'Unit' is not defined in the parent.
    import @ { Unit; };

    union Fruit(Unit apple, Unit banana);
  };
}

fbld-check-error $prg 4:16
