
# A field can have any type that is defined somewhere in the program.
# Which means it cannot have a type that is not defined.
set prg {
  struct Unit();
  struct A(Unit x, Donut y);

  func main( ; Unit) {
    Unit();
  };
}
fblc-check-error $prg 3:20

