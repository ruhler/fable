
# Variables must be declared in scope.

set prg {
  struct Unit();
  union Bool(Unit True, Unit False);

  func foo(Bool x ; Bool) {
    y;
  };

  func main( ; Unit) {
    Unit();
  };
}

expect_malformed $prg main

