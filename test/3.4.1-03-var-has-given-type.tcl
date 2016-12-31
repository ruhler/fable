set prg {
  // Variable have the type they are declared to have.
  struct Unit();
  union Bool(Unit True, Unit False);

  func foo(Bool x ; Unit) {
    x;
  };

  func main( ; Unit) {
    Unit();
  };
}
fblc-check-error $prg 7:5
