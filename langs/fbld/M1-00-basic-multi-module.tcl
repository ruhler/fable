set prg {
  # A basic test case using multiple modules.
  interf UnitI {
    struct Unit();
  };

  module UnitM(UnitI) {
    struct Unit();
  };

  interf MainI {
    import @ { UnitM; };
    func main( ; Unit@UnitM);
  };

  module MainM(MainI) {
    import @ { UnitM; };
    func main( ; Unit@UnitM) {
      Unit@UnitM();
    };
  };
}

fbld-test $prg "main@MainM" {} {
  return Unit@UnitM()
}
