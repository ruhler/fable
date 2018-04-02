set prg {
  # A basic use case of alias.
  module StdLib {
    struct Unit();
  };

  module Aliased = StdLib;

  func main( ; Unit@Aliased) {
    Unit@Aliased();
  };
}

fbld-test $prg "main" {} {
  return Unit@StdLib()
}
