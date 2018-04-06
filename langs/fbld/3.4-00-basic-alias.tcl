set prg {
  # A basic use case of alias.
  interf StdLibI {
    struct Unit();
  };

  module StdLib(StdLibI) {
    struct Unit();
  };

  module Aliased(StdLibI) = StdLib;

  func main( ; Unit@Aliased) {
    Unit@Aliased();
  };
}

fbld-test $prg "main" {} {
  return Unit@StdLib()
}
