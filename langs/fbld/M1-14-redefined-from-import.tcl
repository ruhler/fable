set prg {
  interf UnitI {
    struct Unit();
    func f( ; Unit);
  };

  module UnitM(UnitI) {
    struct Unit();
    func f( ; Unit) {
      Unit();
    };
  };

  interf MainI {
    import @ { UnitM; };
    import UnitM { Unit; };
    func main( ; Unit);
  };

  module MainM(MainI) {
    import @ { UnitM; };
    import UnitM { Unit; f; };
    func main( ; Unit) {
      f();
    };

    # f was already defined via importing
    func f( ; Unit) {
      Unit();
    };
  };
}

fbld-check-error $prg 28:10
