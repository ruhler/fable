set prg {
  module B {
    # The type Blah is not defined.
    struct Bad(Blah x);
  };

  # The main function references an entity in a module with errors.
  func main( ; Bad@B) {
    Bad@B();
  };
}

fbld-check-error $prg 4:16
