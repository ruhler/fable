set prg {
  # Test that we can use an import statement to refer to foreign entities.
  interf UnitI {
    struct Unit();
  };

  module UnitM(UnitI) {
    struct Unit();
  };

  interf MainI {
    import @ { UnitM; };
    import UnitM { Unit; };
    func main( ; Unit);
  };

  module MainM(MainI) {
    import @ { UnitM; };
    import UnitM { Unit; };
    func main( ; Unit) {
      Unit();
    };
  };
}

fbld-test $prg "main@MainM" {} {
  return Unit@UnitM()
}
