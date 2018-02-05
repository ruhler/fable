set prg {
  # A basic test case using a module.
  module StdLib {
    struct Unit();
  };

  func main( ; Unit@StdLib) {
    Unit@StdLib();
  };
}

fbld-test $prg "main" {} {
  return Unit@StdLib()
}
