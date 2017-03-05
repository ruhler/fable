set prg {
  # Variables must be declared in scope.
  struct Unit();
  union Bool(Unit True, Unit False);

  func foo(Bool x ; Bool) {
    y;
  };

  func main( ; Unit) {
    Unit();
  };
}

fblc-check-error $prg 7:5
