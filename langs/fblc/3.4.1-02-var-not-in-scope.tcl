set prg {
  struct Unit();
  union Bool(Unit True, Unit False);

  func foo(Bool x ; Bool) {
    y;  # The variable 'y' has not been declared.
  };

  func main( ; Unit) {
    Unit();
  };
}

fblc-check-error $prg 6:5
