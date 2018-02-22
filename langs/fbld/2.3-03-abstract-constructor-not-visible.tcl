set prg {
  module StdLib {
    abst struct Unit();
  };

  func main( ; Unit@StdLib) {
    # The Unit type cannot be constructed because it is an abstract type.
    Unit@StdLib();
  };
}

fbld-check-error $prg 8:5
