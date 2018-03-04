set prg {
  struct Unit();

  module M {
    import @ { Unit; };

    # Unit was already defined via importing.
    struct Unit();
  };
}

fbld-check-error $prg 8:12
