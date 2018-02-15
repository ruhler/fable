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
    func main( ; Unit);
  };

  module MainM(MainI) {
    import @ { UnitM; };
    import UnitM { Unit; f; };  # f is not defined
    func main( ; Unit) {
      f();
    };
  };
}

skip fbld-check-error $prg 18:26
