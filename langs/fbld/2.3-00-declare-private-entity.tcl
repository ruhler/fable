set prg {
  # Test that we can declare and use a private entity.
  # Does not test that the entity is actually private.
  module StdLib {
    struct Unit();

    priv func MkUnitInternal( ; Unit) {
      Unit();
    };

    func MkUnit( ; Unit) {
      MkUnitInternal();
    };
  };

  func main( ; Unit@StdLib) {
    MkUnit@StdLib();
  };
}

fbld-test $prg "main" {} {
  return Unit@StdLib()
}
