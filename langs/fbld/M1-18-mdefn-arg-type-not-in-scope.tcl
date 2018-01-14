set prg {
  interf UnitI {
    struct Unit();
  };

  module UnitM(UnitI) {
    struct Unit();
  };
   
  interf MainI {
    import @ { UnitM; };
    import UnitM { Unit; };

    struct Foo(Unit a, Unit b);
  };

  module MainM(MainI) {
    # We haven't imported the Unit type.
    struct Foo(Unit a, Unit b);
  };
}

fbld-check-error $prg 19:16
