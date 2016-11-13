

# Variable have the type they are declared to have.

set prg {
  struct Unit();
  union Bool(Unit True, Unit False);

  func foo(Bool x ; Unit) {
    x;
  };

  func main( ; Unit) {
    Unit();
  };
}

expect_malformed $prg main

