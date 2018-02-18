set prg {
  struct Unit();

  module B {
    # Importing the module A means A should be checked first, even though 
    # A shows up later in the source code.
    import @ { A; Unit; };

    func DoB( ; Unit) {
      TA2Unit@A(MkTA@A());
    };
  };

  module A {
    import @ { Unit;};

    struct TA();

    func MkTA( ; TA) {
      TA();
    };

    func TA2Unit(TA x; Unit) {
      Unit();
    };
  };

  func main( ; Unit) {
    DoB@B();
  };
}

fbld-test $prg "main" {} {
  return Unit()
}
