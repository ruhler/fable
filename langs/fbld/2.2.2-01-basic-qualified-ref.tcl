set prg {
  module StdLib {
    struct Unit();

    func mkUnit( ; Unit) {
      Unit();
    };
  };

  # Verify we can access Unit and mkUnit using qualified references.
  func main( ; Unit@StdLib) {
    mkUnit@StdLib();
  };
}

fbld-test $prg "main" {} {
  return Unit@StdLib()
}
