set prg {
  module StdLib {
    struct Unit();
    func MkUnit( ; Unit) {
      Unit();
    };
  };

  module M {
    import @ { StdLib; };

    # Verify we can call a function from another module.
    func MyMkUnit( ; Unit@StdLib) {
      MkUnit@StdLib();
    };
  };

  func main( ; Unit@StdLib) {
    MyMkUnit@M();
  };
}

fbld-test $prg "main" {} {
  return Unit@StdLib()
}
