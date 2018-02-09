set prg {
  module StdLib {
    struct Unit();

    func mkUnit( ; Unit) {
      Unit();
    };
  };

  module Main {
    # Verify we can use qrefs in import statements.
    import @ { Unit@StdLib; Mk=mkUnit@StdLib; };

    func main( ; Unit) {
      Mk();
    };
  };
}

fbld-test $prg "main@Main" {} {
  return Unit@StdLib()
}
