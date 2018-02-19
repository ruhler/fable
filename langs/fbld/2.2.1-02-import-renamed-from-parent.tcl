set prg {
  struct Unit();

  module Food {
    # An entity can be imported under a different name.
    import @ { Donut=Unit; };

    func MkDonut( ; Donut) {
      Donut();
    };
  };

  func main( ; Unit) {
    # That renamed entity can be used where the original one is expected. They
    # are both names for the same type.
    MkDonut@Food();
  };
}

fbld-test $prg "main" {} {
  return Unit()
}
