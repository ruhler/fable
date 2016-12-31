set prg {
  // A field can have any type that is defined somewhere in the program.
  // Which means it cannot have a type that is not defined.
  struct Unit();
  union A(Unit x, Donut y);

  func main( ; A) {
    A:x(Unit());
  };
}
fblc-check-error $prg 5:19
